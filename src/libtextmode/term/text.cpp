#include <cmath>
#include <sstream>
#include "text.h"
#include "../utf8.h"

std::vector<lab_t> create_lab_palette(std::vector<rgb_t>& source_rgbs)
{
    std::vector<lab_t> lab_palette;
    for(auto& rgb_value:source_rgbs) {
        lab_palette.push_back(rgb_value.from_rgb_to_lab());
    }
    return std::move(lab_palette);
}

size_t match_lab(const lab_t& lab_value, const std::vector<lab_t>& lookup_labs)
{
    double value, lowest;
    size_t match;
    for(size_t i = 0; i < lookup_labs.size(); ++i) {
        value = sqrt(pow(lab_value.red - lookup_labs[i].red, 2) + pow(lab_value.green - lookup_labs[i].green, 2) + pow(lab_value.blue - lookup_labs[i].blue, 2));
        if(i == 0 || value < lowest) {
            match = i;
            lowest = value;
        }
    }
    return match;
}

std::vector<size_t> create_lookup_references(std::vector<rgb_t>& source_rgbs, std::vector<rgb_t>& lookup_rgbs)
{
    std::vector<lab_t> source_labs = create_lab_palette(source_rgbs);
    std::vector<lab_t> lookup_labs = create_lab_palette(lookup_rgbs);

    std::vector<size_t> references;
    for(auto& lab_value:source_labs) {
        size_t match = match_lab(lab_value, lookup_labs);
        references.push_back(match);
    }

    return std::move(references);
}

std::vector<size_t> create_lookup(textmode_t& textmode, std::vector<rgb_t>& lookup_rgbs)
{
    switch(textmode.options.palette_type) {
    case palette_type_t::ansi:
    case palette_type_t::ansi_with_truecolor:
    case palette_type_t::binary_text:
    case palette_type_t::custom:
        return create_lookup_references(textmode.image_data.palette.rgb, lookup_rgbs);
    case palette_type_t::truecolor:
    case palette_type_t::none:
        return create_lookup_references(lookup_rgbs, lookup_rgbs);
    }
}

void display_as_text(std::ostream& ostream, textmode_t& textmode)
{
    for(size_t y = 0, i = 0; y < textmode.image_data.rows; ++y) {
        for(size_t x = 0; x < textmode.image_data.columns; ++x, ++i) {
            block_t& block = textmode.image_data.data[i];
            ostream << cp_437_code_to_string(block.code);
        }
        ostream << std::endl;
    }
}

void display_as_ansi(std::ostream& ostream, textmode_t& textmode)
{
    std::vector<rgb_t> ansi_rgb_palette = {
        {0x00, 0x00, 0x00}, {0xaa, 0x00, 0x00}, {0x00, 0xaa, 0x00}, {0xaa, 0X55, 0x00},
        {0x00, 0x00, 0xaa}, {0xaa, 0x00, 0xaa}, {0x00, 0xaa, 0xaa}, {0xaa, 0xaa, 0xaa},

        {0x55, 0x55, 0x55}, {0xff, 0x55, 0x55}, {0x55, 0xff, 0x55}, {0xff, 0xff, 0x55},
        {0x55, 0x55, 0xff}, {0xff, 0x55, 0xff}, {0x55, 0xff, 0xff}, {0xff, 0xff, 0xff}
    };

    uint8_t fg = 7;
    uint8_t bg = 0;
    ostream << "\e[0m";

    if(textmode.options.palette_type == palette_type_t::none) {
        display_as_text(ostream, textmode);
    } else {
        auto lookup = create_lookup(textmode, ansi_rgb_palette);
        auto ansi_labs_palette = create_lab_palette(ansi_rgb_palette);

        for(size_t y = 0, i = 0; y < textmode.image_data.rows; ++y) {
            for(size_t x = 0; x < textmode.image_data.columns; ++x, ++i) {
                block_t& block = textmode.image_data.data[i];
                uint8_t new_fg, new_bg;
                if(block.attr.fg_rgb_mode) {
                    new_fg = match_lab(block.attr.fg_rgb.from_rgb_to_lab(), ansi_labs_palette);
                } else {
                    new_fg = lookup[block.attr.fg];
                }
                if(block.attr.bg_rgb_mode) {
                    new_bg = match_lab(block.attr.bg_rgb.from_rgb_to_lab(), ansi_labs_palette);
                } else {
                    new_bg = lookup[block.attr.bg];
                }
                if(new_fg != fg || new_bg != bg) {
                    std::vector<uint8_t> codes;

                    if(new_fg != fg) {
                        if(new_fg >= 8 && fg < 8) {
                            codes.push_back(1);
                        } else if(new_fg < 8 && fg >=8) {
                            codes.push_back(22);
                        }
                        if(new_fg < 8) {
                            codes.push_back(30 + new_fg);
                        } else {
                            codes.push_back(30 + new_fg - 8);
                        }
                        fg = new_fg;
                    }

                    if(new_bg != bg) {
                        if(new_bg >= 8 && bg < 8) {
                            codes.push_back(5);
                        } else if(new_bg < 8 && bg >=8) {
                            codes.push_back(25);
                        }
                        if(new_bg < 8) {
                            codes.push_back(40 + new_bg);
                        } else {
                            codes.push_back(40 + new_bg - 8);
                        }
                        bg = new_bg;
                    }

                    ostream << "\e[";
                    for(const auto& code:codes) {
                        ostream << int(code);
                        if(code != codes.back()) {
                            ostream << ";";
                        }
                    }
                    ostream << "m";
                }
                ostream << cp_437_code_to_string(block.code);
            }
            if(bg != 0 && bg != 8) {
                ostream << "\e[";
                if(bg > 8) {
                    ostream << "25;";
                }
                ostream << "40m";
                bg = 0;
            }
            ostream << std::endl;
        }
        if(fg != 7 || bg != 0) {
            ostream << "\e[0m";
        }
    }
}

std::vector<rgb_t> xterm256_rgb_palette = {
    {0x00, 0x00, 0x00}, {0x80, 0x00, 0x00}, {0x00, 0x80, 0x00}, {0x80, 0x80, 0x00},
    {0x00, 0x00, 0x80}, {0x80, 0x00, 0x80}, {0x00, 0x80, 0x80}, {0xc0, 0xc0, 0xc0},
    {0x80, 0x80, 0x80}, {0xff, 0x00, 0x00}, {0x00, 0xff, 0x00}, {0xff, 0xff, 0x00},
    {0x00, 0x00, 0xff}, {0xff, 0x00, 0xff}, {0x00, 0xff, 0xff}, {0xff, 0xff, 0xff},
    {0x00, 0x00, 0x00}, {0x00, 0x00, 0x5f}, {0x00, 0x00, 0x87}, {0x00, 0x00, 0xaf},
    {0x00, 0x00, 0xd7}, {0x00, 0x00, 0xff}, {0x00, 0x5f, 0x00}, {0x00, 0x5f, 0x5f},
    {0x00, 0x5f, 0x87}, {0x00, 0x5f, 0xaf}, {0x00, 0x5f, 0xd7}, {0x00, 0x5f, 0xff},
    {0x00, 0x87, 0x00}, {0x00, 0x87, 0x5f}, {0x00, 0x87, 0x87}, {0x00, 0x87, 0xaf},
    {0x00, 0x87, 0xd7}, {0x00, 0x87, 0xff}, {0x00, 0xaf, 0x00}, {0x00, 0xaf, 0x5f},
    {0x00, 0xaf, 0x87}, {0x00, 0xaf, 0xaf}, {0x00, 0xaf, 0xd7}, {0x00, 0xaf, 0xff},
    {0x00, 0xd7, 0x00}, {0x00, 0xd7, 0x5f}, {0x00, 0xd7, 0x87}, {0x00, 0xd7, 0xaf},
    {0x00, 0xd7, 0xd7}, {0x00, 0xd7, 0xff}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x5f},
    {0x00, 0xff, 0x87}, {0x00, 0xff, 0xaf}, {0x00, 0xff, 0xd7}, {0x00, 0xff, 0xff},
    {0x5f, 0x00, 0x00}, {0x5f, 0x00, 0x5f}, {0x5f, 0x00, 0x87}, {0x5f, 0x00, 0xaf},
    {0x5f, 0x00, 0xd7}, {0x5f, 0x00, 0xff}, {0x5f, 0x5f, 0x00}, {0x5f, 0x5f, 0x5f},
    {0x5f, 0x5f, 0x87}, {0x5f, 0x5f, 0xaf}, {0x5f, 0x5f, 0xd7}, {0x5f, 0x5f, 0xff},
    {0x5f, 0x87, 0x00}, {0x5f, 0x87, 0x5f}, {0x5f, 0x87, 0x87}, {0x5f, 0x87, 0xaf},
    {0x5f, 0x87, 0xd7}, {0x5f, 0x87, 0xff}, {0x5f, 0xaf, 0x00}, {0x5f, 0xaf, 0x5f},
    {0x5f, 0xaf, 0x87}, {0x5f, 0xaf, 0xaf}, {0x5f, 0xaf, 0xd7}, {0x5f, 0xaf, 0xff},
    {0x5f, 0xd7, 0x00}, {0x5f, 0xd7, 0x5f}, {0x5f, 0xd7, 0x87}, {0x5f, 0xd7, 0xaf},
    {0x5f, 0xd7, 0xd7}, {0x5f, 0xd7, 0xff}, {0x5f, 0xff, 0x00}, {0x5f, 0xff, 0x5f},
    {0x5f, 0xff, 0x87}, {0x5f, 0xff, 0xaf}, {0x5f, 0xff, 0xd7}, {0x5f, 0xff, 0xff},
    {0x87, 0x00, 0x00}, {0x87, 0x00, 0x5f}, {0x87, 0x00, 0x87}, {0x87, 0x00, 0xaf},
    {0x87, 0x00, 0xd7}, {0x87, 0x00, 0xff}, {0x87, 0x5f, 0x00}, {0x87, 0x5f, 0x5f},
    {0x87, 0x5f, 0x87}, {0x87, 0x5f, 0xaf}, {0x87, 0x5f, 0xd7}, {0x87, 0x5f, 0xff},
    {0x87, 0x87, 0x00}, {0x87, 0x87, 0x5f}, {0x87, 0x87, 0x87}, {0x87, 0x87, 0xaf},
    {0x87, 0x87, 0xd7}, {0x87, 0x87, 0xff}, {0x87, 0xaf, 0x00}, {0x87, 0xaf, 0x5f},
    {0x87, 0xaf, 0x87}, {0x87, 0xaf, 0xaf}, {0x87, 0xaf, 0xd7}, {0x87, 0xaf, 0xff},
    {0x87, 0xd7, 0x00}, {0x87, 0xd7, 0x5f}, {0x87, 0xd7, 0x87}, {0x87, 0xd7, 0xaf},
    {0x87, 0xd7, 0xd7}, {0x87, 0xd7, 0xff}, {0x87, 0xff, 0x00}, {0x87, 0xff, 0x5f},
    {0x87, 0xff, 0x87}, {0x87, 0xff, 0xaf}, {0x87, 0xff, 0xd7}, {0x87, 0xff, 0xff},
    {0xaf, 0x00, 0x00}, {0xaf, 0x00, 0x5f}, {0xaf, 0x00, 0x87}, {0xaf, 0x00, 0xaf},
    {0xaf, 0x00, 0xd7}, {0xaf, 0x00, 0xff}, {0xaf, 0x5f, 0x00}, {0xaf, 0x5f, 0x5f},
    {0xaf, 0x5f, 0x87}, {0xaf, 0x5f, 0xaf}, {0xaf, 0x5f, 0xd7}, {0xaf, 0x5f, 0xff},
    {0xaf, 0x87, 0x00}, {0xaf, 0x87, 0x5f}, {0xaf, 0x87, 0x87}, {0xaf, 0x87, 0xaf},
    {0xaf, 0x87, 0xd7}, {0xaf, 0x87, 0xff}, {0xaf, 0xaf, 0x00}, {0xaf, 0xaf, 0x5f},
    {0xaf, 0xaf, 0x87}, {0xaf, 0xaf, 0xaf}, {0xaf, 0xaf, 0xd7}, {0xaf, 0xaf, 0xff},
    {0xaf, 0xd7, 0x00}, {0xaf, 0xd7, 0x5f}, {0xaf, 0xd7, 0x87}, {0xaf, 0xd7, 0xaf},
    {0xaf, 0xd7, 0xd7}, {0xaf, 0xd7, 0xff}, {0xaf, 0xff, 0x00}, {0xaf, 0xff, 0x5f},
    {0xaf, 0xff, 0x87}, {0xaf, 0xff, 0xaf}, {0xaf, 0xff, 0xd7}, {0xaf, 0xff, 0xff},
    {0xd7, 0x00, 0x00}, {0xd7, 0x00, 0x5f}, {0xd7, 0x00, 0x87}, {0xd7, 0x00, 0xaf},
    {0xd7, 0x00, 0xd7}, {0xd7, 0x00, 0xff}, {0xd7, 0x5f, 0x00}, {0xd7, 0x5f, 0x5f},
    {0xd7, 0x5f, 0x87}, {0xd7, 0x5f, 0xaf}, {0xd7, 0x5f, 0xd7}, {0xd7, 0x5f, 0xff},
    {0xd7, 0x87, 0x00}, {0xd7, 0x87, 0x5f}, {0xd7, 0x87, 0x87}, {0xd7, 0x87, 0xaf},
    {0xd7, 0x87, 0xd7}, {0xd7, 0x87, 0xff}, {0xd7, 0xaf, 0x00}, {0xd7, 0xaf, 0x5f},
    {0xd7, 0xaf, 0x87}, {0xd7, 0xaf, 0xaf}, {0xd7, 0xaf, 0xd7}, {0xd7, 0xaf, 0xff},
    {0xd7, 0xd7, 0x00}, {0xd7, 0xd7, 0x5f}, {0xd7, 0xd7, 0x87}, {0xd7, 0xd7, 0xaf},
    {0xd7, 0xd7, 0xd7}, {0xd7, 0xd7, 0xff}, {0xd7, 0xff, 0x00}, {0xd7, 0xff, 0x5f},
    {0xd7, 0xff, 0x87}, {0xd7, 0xff, 0xaf}, {0xd7, 0xff, 0xd7}, {0xd7, 0xff, 0xff},
    {0xff, 0x00, 0x00}, {0xff, 0x00, 0x5f}, {0xff, 0x00, 0x87}, {0xff, 0x00, 0xaf},
    {0xff, 0x00, 0xd7}, {0xff, 0x00, 0xff}, {0xff, 0x5f, 0x00}, {0xff, 0x5f, 0x5f},
    {0xff, 0x5f, 0x87}, {0xff, 0x5f, 0xaf}, {0xff, 0x5f, 0xd7}, {0xff, 0x5f, 0xff},
    {0xff, 0x87, 0x00}, {0xff, 0x87, 0x5f}, {0xff, 0x87, 0x87}, {0xff, 0x87, 0xaf},
    {0xff, 0x87, 0xd7}, {0xff, 0x87, 0xff}, {0xff, 0xaf, 0x00}, {0xff, 0xaf, 0x5f},
    {0xff, 0xaf, 0x87}, {0xff, 0xaf, 0xaf}, {0xff, 0xaf, 0xd7}, {0xff, 0xaf, 0xff},
    {0xff, 0xd7, 0x00}, {0xff, 0xd7, 0x5f}, {0xff, 0xd7, 0x87}, {0xff, 0xd7, 0xaf},
    {0xff, 0xd7, 0xd7}, {0xff, 0xd7, 0xff}, {0xff, 0xff, 0x00}, {0xff, 0xff, 0x5f},
    {0xff, 0xff, 0x87}, {0xff, 0xff, 0xaf}, {0xff, 0xff, 0xd7}, {0xff, 0xff, 0xff},
    {0x08, 0x08, 0x08}, {0x12, 0x12, 0x12}, {0x1c, 0x1c, 0x1c}, {0x26, 0x26, 0x26},
    {0x30, 0x30, 0x30}, {0x3a, 0x3a, 0x3a}, {0x44, 0x44, 0x44}, {0x4e, 0x4e, 0x4e},
    {0x58, 0x58, 0x58}, {0x62, 0x62, 0x62}, {0x6c, 0x6c, 0x6c}, {0x76, 0x76, 0x76},
    {0x80, 0x80, 0x80}, {0x8a, 0x8a, 0x8a}, {0x94, 0x94, 0x94}, {0x9e, 0x9e, 0x9e},
    {0xa8, 0xa8, 0xa8}, {0xb2, 0xb2, 0xb2}, {0xbc, 0xbc, 0xbc}, {0xc6, 0xc6, 0xc6},
    {0xd0, 0xd0, 0xd0}, {0xda, 0xda, 0xda}, {0xe4, 0xe4, 0xe4}, {0xee, 0xee, 0xee}
};

void display_as_xterm256(std::ostream& ostream, textmode_t& textmode)
{
    uint8_t fg = 7;
    uint8_t bg = 0;
    bool blink = false;
    ostream << "\e[0m";

    if(textmode.options.palette_type == palette_type_t::none) {
        display_as_text(ostream, textmode);
    } else {
        auto lookup = create_lookup(textmode, xterm256_rgb_palette);
        auto xterm256_labs_palette = create_lab_palette(xterm256_rgb_palette);

        for(size_t y = 0, i = 0; y < textmode.image_data.rows; ++y) {
            for(size_t x = 0; x < textmode.image_data.columns; ++x, ++i) {
                block_t& block = textmode.image_data.data[i];
                uint8_t new_fg, new_bg;
                bool new_blink;
                if(block.attr.fg_rgb_mode) {
                    new_fg = match_lab(block.attr.fg_rgb.from_rgb_to_lab(), xterm256_labs_palette);
                } else {
                    new_fg = lookup[block.attr.fg];
                }
                if(block.attr.bg_rgb_mode) {
                    new_bg = match_lab(block.attr.bg_rgb.from_rgb_to_lab(), xterm256_labs_palette);
                } else {
                    new_blink = (textmode.options.non_blink == non_blink_t::off) && (block.attr.bg >= 8);
                    if(new_blink) {
                        new_bg = lookup[block.attr.bg - 8];
                    } else {
                        new_bg = lookup[block.attr.bg];
                    }
                }
                if(new_fg != fg || new_bg != bg) {
                    std::vector<uint8_t> codes;

                    if(new_fg != fg) {
                        fg = new_fg;
                        ostream << "\e[38;5;" << int(fg) << "m";
                    }

                    if(new_bg != bg) {
                        bg = new_bg;
                        ostream << "\e[48;5;" << int(bg) << "m";
                    }

                    if(blink != new_blink) {
                        blink = new_blink;
                        ostream << (blink ? "\e[5m" : "\e[25m");
                    }
                }
                ostream << cp_437_code_to_string(block.code);
            }
            if(bg != 0) {
                ostream << "\e[48;5;0m";
                bg = 0;
            }
            ostream << std::endl;
        }
        if(fg != 7 || bg != 0) {
            ostream << "\e[0m";
        }
    }
}

inline void display_24Bit_sequence(std::ostream& ostream, const std::string& prefix, const rgb_t& rgb_value)
{
    ostream << "\e[" << prefix << ";2;" << int(rgb_value.red) << ";" << int(rgb_value.green) << ";" << int(rgb_value.blue) << "m";
}

void display_as_xterm24Bit(std::ostream& ostream, textmode_t& textmode)
{
    rgb_t white = {170, 170, 170};
    rgb_t black = {0, 0, 0};

    rgb_t fg = white;
    rgb_t bg = black;
    bool blink = false;

    ostream << "\e[0m";
    display_24Bit_sequence(ostream, "38", fg);
    display_24Bit_sequence(ostream, "48", bg);

    if(textmode.options.palette_type == palette_type_t::none) {
        display_as_text(ostream, textmode);
    } else {
        for(size_t y = 0, i = 0; y < textmode.image_data.rows; ++y) {
            for(size_t x = 0; x < textmode.image_data.columns; ++x, ++i) {
                block_t& block = textmode.image_data.data[i];
                rgb_t new_fg, new_bg;
                bool new_blink;
                if(block.attr.fg_rgb_mode) {
                    new_fg = block.attr.fg_rgb;
                } else {
                    new_fg = textmode.image_data.palette.rgb[block.attr.fg];
                }
                if(block.attr.bg_rgb_mode) {
                    new_bg = block.attr.bg_rgb;
                } else {
                    new_blink = (textmode.options.non_blink == non_blink_t::off) && (block.attr.bg >= 8);
                    if(new_blink) {
                        new_bg = textmode.image_data.palette.rgb[block.attr.bg - 8];
                    } else {
                        new_bg = textmode.image_data.palette.rgb[block.attr.bg];
                    }
                }
                if(new_fg != fg || new_bg != bg) {
                    std::vector<uint8_t> codes;

                    if(new_fg != fg) {
                        fg = new_fg;
                        display_24Bit_sequence(ostream, "38", fg);
                    }

                    if(new_bg != bg) {
                        bg = new_bg;
                        display_24Bit_sequence(ostream, "48", bg);
                    }

                    if(blink != new_blink) {
                        blink = new_blink;
                        ostream << (blink ? "\e[5m" : "\e[25m");
                    }
                }
                ostream << cp_437_code_to_string(block.code);
            }
            if(bg != black) {
                bg = black;
                display_24Bit_sequence(ostream, "48", bg);
            }
            ostream << std::endl;
        }
        ostream << "\e[0m";
    }
}