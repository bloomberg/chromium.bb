// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "base/values.h"
#include "base/json/json_reader.h"

class BrowserThemeProviderTest : public ::testing::Test {
 public:
  // Transformation for link underline colors.
  SkColor BuildThirdOpacity(SkColor color_link) {
    return SkColorSetA(color_link, SkColorGetA(color_link) / 3);
  }

  // Returns a mapping from each COLOR_* constant to the default value for this
  // constant. Callers get this map, and then modify expected values and then
  // run the resulting thing through VerifyColorMap().
  std::map<int, SkColor> GetDefaultColorMap() {
    std::map<int, SkColor> colors;
    colors[BrowserThemeProvider::COLOR_FRAME] =
        BrowserThemeProvider::kDefaultColorFrame;
    colors[BrowserThemeProvider::COLOR_FRAME_INACTIVE] =
        BrowserThemeProvider::kDefaultColorFrameInactive;
    colors[BrowserThemeProvider::COLOR_FRAME_INCOGNITO] =
        BrowserThemeProvider::kDefaultColorFrameIncognito;
    colors[BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE] =
        BrowserThemeProvider::kDefaultColorFrameIncognitoInactive;
    colors[BrowserThemeProvider::COLOR_TOOLBAR] =
        BrowserThemeProvider::kDefaultColorToolbar;
    colors[BrowserThemeProvider::COLOR_TAB_TEXT] =
        BrowserThemeProvider::kDefaultColorTabText;
    colors[BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT] =
        BrowserThemeProvider::kDefaultColorBackgroundTabText;
    colors[BrowserThemeProvider::COLOR_BOOKMARK_TEXT] =
        BrowserThemeProvider::kDefaultColorBookmarkText;
    colors[BrowserThemeProvider::COLOR_NTP_BACKGROUND] =
        BrowserThemeProvider::kDefaultColorNTPBackground;
    colors[BrowserThemeProvider::COLOR_NTP_TEXT] =
        BrowserThemeProvider::kDefaultColorNTPText;
    colors[BrowserThemeProvider::COLOR_NTP_LINK] =
        BrowserThemeProvider::kDefaultColorNTPLink;
    colors[BrowserThemeProvider::COLOR_NTP_LINK_UNDERLINE] =
        BuildThirdOpacity(BrowserThemeProvider::kDefaultColorNTPLink);
    colors[BrowserThemeProvider::COLOR_NTP_HEADER] =
        BrowserThemeProvider::kDefaultColorNTPHeader;
    colors[BrowserThemeProvider::COLOR_NTP_SECTION] =
        BrowserThemeProvider::kDefaultColorNTPSection;
    colors[BrowserThemeProvider::COLOR_NTP_SECTION_TEXT] =
        BrowserThemeProvider::kDefaultColorNTPSectionText;
    colors[BrowserThemeProvider::COLOR_NTP_SECTION_LINK] =
        BrowserThemeProvider::kDefaultColorNTPSectionLink;
    colors[BrowserThemeProvider::COLOR_NTP_SECTION_LINK_UNDERLINE] =
        BuildThirdOpacity(BrowserThemeProvider::kDefaultColorNTPSectionLink);
    colors[BrowserThemeProvider::COLOR_CONTROL_BACKGROUND] =
        BrowserThemeProvider::kDefaultColorControlBackground;
    colors[BrowserThemeProvider::COLOR_BUTTON_BACKGROUND] =
        BrowserThemeProvider::kDefaultColorButtonBackground;

    return colors;
  }

  void VerifyColorMap(const std::map<int, SkColor>& color_map) {
    for (std::map<int, SkColor>::const_iterator it = color_map.begin();
         it != color_map.end(); ++it) {
      EXPECT_EQ(it->second, provider_.GetColor(it->first));
    }
  }

  void LoadColorJSON(const std::string& json) {
    scoped_ptr<Value> value(base::JSONReader::Read(json, false));
    ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
    provider_.SetColorData(static_cast<DictionaryValue*>(value.get()));
  }

  void LoadTintJSON(const std::string& json) {
    scoped_ptr<Value> value(base::JSONReader::Read(json, false));
    ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
    provider_.SetTintData(static_cast<DictionaryValue*>(value.get()));
  }

  void GenerateFrameColors() {
    provider_.GenerateFrameColors();
  }

  BrowserThemeProvider provider_;
};

TEST_F(BrowserThemeProviderTest, AlignmentConversion) {
  // Verify that we get out what we put in.
  std::string top_left = "top left";
  int alignment = BrowserThemeProvider::StringToAlignment(top_left);
  EXPECT_EQ(BrowserThemeProvider::ALIGN_TOP | BrowserThemeProvider::ALIGN_LEFT,
            alignment);
  EXPECT_EQ(top_left, BrowserThemeProvider::AlignmentToString(alignment));

  alignment = BrowserThemeProvider::StringToAlignment("top");
  EXPECT_EQ(BrowserThemeProvider::ALIGN_TOP, alignment);
  EXPECT_EQ("top", BrowserThemeProvider::AlignmentToString(alignment));

  alignment = BrowserThemeProvider::StringToAlignment("left");
  EXPECT_EQ(BrowserThemeProvider::ALIGN_LEFT, alignment);
  EXPECT_EQ("left", BrowserThemeProvider::AlignmentToString(alignment));

  alignment = BrowserThemeProvider::StringToAlignment("right");
  EXPECT_EQ(BrowserThemeProvider::ALIGN_RIGHT, alignment);
  EXPECT_EQ("right", BrowserThemeProvider::AlignmentToString(alignment));

  alignment = BrowserThemeProvider::StringToAlignment("righttopbottom");
  EXPECT_EQ(BrowserThemeProvider::ALIGN_CENTER, alignment);
  EXPECT_EQ("", BrowserThemeProvider::AlignmentToString(alignment));
}

TEST_F(BrowserThemeProviderTest, AlignmentConversionInput) {
  // Verify that we output in an expected format.
  int alignment = BrowserThemeProvider::StringToAlignment("right bottom");
  EXPECT_EQ("bottom right", BrowserThemeProvider::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = BrowserThemeProvider::StringToAlignment("new zealand");
  EXPECT_EQ("", BrowserThemeProvider::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = BrowserThemeProvider::StringToAlignment("new zealand top");
  EXPECT_EQ("top", BrowserThemeProvider::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = BrowserThemeProvider::StringToAlignment("new zealandtop");
  EXPECT_EQ("", BrowserThemeProvider::AlignmentToString(alignment));
}

TEST_F(BrowserThemeProviderTest, ColorSanityCheck) {
  // Make sure that BrowserThemeProvider returns all the default colors if it
  // isn't provided any color overrides.
  std::map<int, SkColor> colors = GetDefaultColorMap();
  VerifyColorMap(colors);
}

TEST_F(BrowserThemeProviderTest, DeriveUnderlineLinkColor) {
  // If we specify a link color, but don't specify the underline color, the
  // theme provider should create one.
  std::string color_json = "{ \"ntp_link\": [128, 128, 128, 126],"
                           "  \"ntp_section_link\": [128, 128, 128, 126] }";
  LoadColorJSON(color_json);

  std::map<int, SkColor> colors = GetDefaultColorMap();
  SkColor link_color = SkColorSetARGB(126, 128, 128, 128);
  colors[BrowserThemeProvider::COLOR_NTP_LINK] = link_color;
  colors[BrowserThemeProvider::COLOR_NTP_LINK_UNDERLINE] =
      BuildThirdOpacity(link_color);
  colors[BrowserThemeProvider::COLOR_NTP_SECTION_LINK] = link_color;
  colors[BrowserThemeProvider::COLOR_NTP_SECTION_LINK_UNDERLINE] =
      BuildThirdOpacity(link_color);

  VerifyColorMap(colors);
}

TEST_F(BrowserThemeProviderTest, ProvideUnderlineLinkColor) {
  // If we specify the underline color, it shouldn't try to generate one.x
  std::string color_json = "{ \"ntp_link\": [128, 128, 128],"
                           "  \"ntp_link_underline\": [255, 255, 255],"
                           "  \"ntp_section_link\": [128, 128, 128],"
                           "  \"ntp_section_link_underline\": [255, 255, 255]"
                           "}";
  LoadColorJSON(color_json);

  std::map<int, SkColor> colors = GetDefaultColorMap();
  SkColor link_color = SkColorSetRGB(128, 128, 128);
  SkColor underline_color = SkColorSetRGB(255, 255, 255);
  colors[BrowserThemeProvider::COLOR_NTP_LINK] = link_color;
  colors[BrowserThemeProvider::COLOR_NTP_LINK_UNDERLINE] = underline_color;
  colors[BrowserThemeProvider::COLOR_NTP_SECTION_LINK] = link_color;
  colors[BrowserThemeProvider::COLOR_NTP_SECTION_LINK_UNDERLINE] =
      underline_color;

  VerifyColorMap(colors);
}

TEST_F(BrowserThemeProviderTest, UseSectionColorAsNTPHeader) {
  std::string color_json = "{ \"ntp_section\": [190, 190, 190] }";
  LoadColorJSON(color_json);

  std::map<int, SkColor> colors = GetDefaultColorMap();
  SkColor ntp_color = SkColorSetRGB(190, 190, 190);
  colors[BrowserThemeProvider::COLOR_NTP_HEADER] = ntp_color;
  colors[BrowserThemeProvider::COLOR_NTP_SECTION] = ntp_color;
  VerifyColorMap(colors);
}

TEST_F(BrowserThemeProviderTest, ProvideNtpHeaderColor) {
  std::string color_json = "{ \"ntp_header\": [120, 120, 120], "
                           "  \"ntp_section\": [190, 190, 190] }";
  LoadColorJSON(color_json);

  std::map<int, SkColor> colors = GetDefaultColorMap();
  SkColor ntp_header = SkColorSetRGB(120, 120, 120);
  SkColor ntp_section = SkColorSetRGB(190, 190, 190);
  colors[BrowserThemeProvider::COLOR_NTP_HEADER] = ntp_header;
  colors[BrowserThemeProvider::COLOR_NTP_SECTION] = ntp_section;
  VerifyColorMap(colors);
}

TEST_F(BrowserThemeProviderTest, DefaultTintingDefaultColors) {
  // Default tints for buttons and frames...are no tints! So make sure that
  // when we try to generate frame colors, we end up with the same.
  GenerateFrameColors();

  std::map<int, SkColor> colors = GetDefaultColorMap();
  colors[BrowserThemeProvider::COLOR_FRAME] =
      HSLShift(BrowserThemeProvider::kDefaultColorFrame,
               BrowserThemeProvider::kDefaultTintFrame);
  colors[BrowserThemeProvider::COLOR_FRAME_INACTIVE] =
      HSLShift(BrowserThemeProvider::kDefaultColorFrame,
               BrowserThemeProvider::kDefaultTintFrameInactive);
  colors[BrowserThemeProvider::COLOR_FRAME_INCOGNITO] =
      HSLShift(BrowserThemeProvider::kDefaultColorFrame,
               BrowserThemeProvider::kDefaultTintFrameIncognito);
  colors[BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE] =
      HSLShift(BrowserThemeProvider::kDefaultColorFrame,
               BrowserThemeProvider::kDefaultTintFrameIncognitoInactive);
  VerifyColorMap(colors);
}

// TODO(erg): Test more tinting combinations. For example, with non-default
// colors or when providing tints.
