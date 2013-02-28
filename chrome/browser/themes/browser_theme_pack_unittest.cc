// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/browser_theme_pack.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/themes/theme_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "content/public/test/test_browser_thread.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_utils.h"

using content::BrowserThread;
using extensions::Extension;

class BrowserThemePackTest : public ::testing::Test {
 public:
  BrowserThemePackTest()
      : message_loop(),
        fake_ui_thread(BrowserThread::UI, &message_loop),
        fake_file_thread(BrowserThread::FILE, &message_loop),
        theme_pack_(new BrowserThemePack) {
  }

  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
    extensions::ManifestHandler::Register(
        extension_manifest_keys::kTheme,
        make_linked_ptr(new extensions::ThemeHandler));
  }

  // Transformation for link underline colors.
  SkColor BuildThirdOpacity(SkColor color_link) {
    return SkColorSetA(color_link, SkColorGetA(color_link) / 3);
  }

  void GenerateDefaultFrameColor(std::map<int, SkColor>* colors,
                                 int color, int tint) {
    (*colors)[color] = HSLShift(
        ThemeProperties::GetDefaultColor(
            ThemeProperties::COLOR_FRAME),
        ThemeProperties::GetDefaultTint(tint));
  }

  // Returns a mapping from each COLOR_* constant to the default value for this
  // constant. Callers get this map, and then modify expected values and then
  // run the resulting thing through VerifyColorMap().
  std::map<int, SkColor> GetDefaultColorMap() {
    std::map<int, SkColor> colors;
    for (int i = ThemeProperties::COLOR_FRAME;
         i <= ThemeProperties::COLOR_BUTTON_BACKGROUND; ++i) {
      colors[i] = ThemeProperties::GetDefaultColor(i);
    }

    GenerateDefaultFrameColor(&colors, ThemeProperties::COLOR_FRAME,
                              ThemeProperties::TINT_FRAME);
    GenerateDefaultFrameColor(&colors,
                              ThemeProperties::COLOR_FRAME_INACTIVE,
                              ThemeProperties::TINT_FRAME_INACTIVE);
    GenerateDefaultFrameColor(&colors,
                              ThemeProperties::COLOR_FRAME_INCOGNITO,
                              ThemeProperties::TINT_FRAME_INCOGNITO);
    GenerateDefaultFrameColor(
        &colors,
        ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE,
        ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE);

    return colors;
  }

  void VerifyColorMap(const std::map<int, SkColor>& color_map) {
    for (std::map<int, SkColor>::const_iterator it = color_map.begin();
         it != color_map.end(); ++it) {
      SkColor color = ThemeProperties::GetDefaultColor(it->first);
      theme_pack_->GetColor(it->first, &color);
      EXPECT_EQ(it->second, color) << "Color id = " << it->first;
    }
  }

  void LoadColorJSON(const std::string& json) {
    scoped_ptr<Value> value(base::JSONReader::Read(json));
    ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
    LoadColorDictionary(static_cast<DictionaryValue*>(value.get()));
  }

  void LoadColorDictionary(DictionaryValue* value) {
    theme_pack_->BuildColorsFromJSON(value);
  }

  void LoadTintJSON(const std::string& json) {
    scoped_ptr<Value> value(base::JSONReader::Read(json));
    ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
    LoadTintDictionary(static_cast<DictionaryValue*>(value.get()));
  }

  void LoadTintDictionary(DictionaryValue* value) {
    theme_pack_->BuildTintsFromJSON(value);
  }

  void LoadDisplayPropertiesJSON(const std::string& json) {
    scoped_ptr<Value> value(base::JSONReader::Read(json));
    ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
    LoadDisplayPropertiesDictionary(static_cast<DictionaryValue*>(value.get()));
  }

  void LoadDisplayPropertiesDictionary(DictionaryValue* value) {
    theme_pack_->BuildDisplayPropertiesFromJSON(value);
  }

  void ParseImageNamesJSON(const std::string& json,
                           std::map<int, base::FilePath>* out_file_paths) {
    scoped_ptr<Value> value(base::JSONReader::Read(json));
    ASSERT_TRUE(value->IsType(Value::TYPE_DICTIONARY));
    ParseImageNamesDictionary(static_cast<DictionaryValue*>(value.get()),
                              out_file_paths);
  }

  void ParseImageNamesDictionary(
      DictionaryValue* value,
      std::map<int, base::FilePath>* out_file_paths) {
    theme_pack_->ParseImageNamesFromJSON(value, base::FilePath(), out_file_paths);

    // Build the source image list for HasCustomImage().
    theme_pack_->BuildSourceImagesArray(*out_file_paths);
  }

  bool LoadRawBitmapsTo(const std::map<int, base::FilePath>& out_file_paths) {
    return theme_pack_->LoadRawBitmapsTo(out_file_paths,
                                         &theme_pack_->images_on_ui_thread_);
  }

  base::FilePath GetStarGazingPath() {
    base::FilePath test_path;
    if (!PathService::Get(chrome::DIR_TEST_DATA, &test_path)) {
      NOTREACHED();
      return test_path;
    }

    test_path = test_path.AppendASCII("profiles");
    test_path = test_path.AppendASCII("profile_with_complex_theme");
    test_path = test_path.AppendASCII("Default");
    test_path = test_path.AppendASCII("Extensions");
    test_path = test_path.AppendASCII("mblmlcbknbnfebdfjnolmcapmdofhmme");
    test_path = test_path.AppendASCII("1.1");
    return base::FilePath(test_path);
  }

  // Verifies the data in star gazing. We do this multiple times for different
  // BrowserThemePack objects to make sure it works in generated and mmapped
  // mode correctly.
  void VerifyStarGazing(BrowserThemePack* pack) {
    // First check that values we know exist, exist.
    SkColor color;
    EXPECT_TRUE(pack->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT,
                               &color));
    EXPECT_EQ(SK_ColorBLACK, color);

    EXPECT_TRUE(pack->GetColor(ThemeProperties::COLOR_NTP_BACKGROUND,
                               &color));
    EXPECT_EQ(SkColorSetRGB(57, 137, 194), color);

    color_utils::HSL expected = { 0.6, 0.553, 0.5 };
    color_utils::HSL actual;
    EXPECT_TRUE(pack->GetTint(ThemeProperties::TINT_BUTTONS, &actual));
    EXPECT_DOUBLE_EQ(expected.h, actual.h);
    EXPECT_DOUBLE_EQ(expected.s, actual.s);
    EXPECT_DOUBLE_EQ(expected.l, actual.l);

    int val;
    EXPECT_TRUE(pack->GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_ALIGNMENT, &val));
    EXPECT_EQ(ThemeProperties::ALIGN_TOP, val);

    // The stargazing theme defines the following images:
    EXPECT_TRUE(pack->HasCustomImage(IDR_THEME_BUTTON_BACKGROUND));
    EXPECT_TRUE(pack->HasCustomImage(IDR_THEME_FRAME));
    EXPECT_TRUE(pack->HasCustomImage(IDR_THEME_NTP_BACKGROUND));
    EXPECT_TRUE(pack->HasCustomImage(IDR_THEME_TAB_BACKGROUND));
    EXPECT_TRUE(pack->HasCustomImage(IDR_THEME_TOOLBAR));
    EXPECT_TRUE(pack->HasCustomImage(IDR_THEME_WINDOW_CONTROL_BACKGROUND));

    // Here are a few images that we shouldn't expect because even though
    // they're included in the theme pack, they were autogenerated and
    // therefore shouldn't show up when calling HasCustomImage().
    EXPECT_FALSE(pack->HasCustomImage(IDR_THEME_FRAME_INACTIVE));
    EXPECT_FALSE(pack->HasCustomImage(IDR_THEME_FRAME_INCOGNITO));
    EXPECT_FALSE(pack->HasCustomImage(IDR_THEME_FRAME_INCOGNITO_INACTIVE));
    EXPECT_FALSE(pack->HasCustomImage(IDR_THEME_TAB_BACKGROUND_INCOGNITO));

    // Make sure we don't have phantom data.
    EXPECT_FALSE(pack->GetColor(ThemeProperties::COLOR_CONTROL_BACKGROUND,
                                &color));
    EXPECT_FALSE(pack->GetTint(ThemeProperties::TINT_FRAME, &actual));
  }

  MessageLoop message_loop;
  content::TestBrowserThread fake_ui_thread;
  content::TestBrowserThread fake_file_thread;

  scoped_refptr<BrowserThemePack> theme_pack_;
};


TEST_F(BrowserThemePackTest, DeriveUnderlineLinkColor) {
  // If we specify a link color, but don't specify the underline color, the
  // theme provider should create one.
  std::string color_json = "{ \"ntp_link\": [128, 128, 128],"
                           "  \"ntp_section_link\": [128, 128, 128] }";
  LoadColorJSON(color_json);

  std::map<int, SkColor> colors = GetDefaultColorMap();
  SkColor link_color = SkColorSetRGB(128, 128, 128);
  colors[ThemeProperties::COLOR_NTP_LINK] = link_color;
  colors[ThemeProperties::COLOR_NTP_LINK_UNDERLINE] =
      BuildThirdOpacity(link_color);
  colors[ThemeProperties::COLOR_NTP_SECTION_LINK] = link_color;
  colors[ThemeProperties::COLOR_NTP_SECTION_LINK_UNDERLINE] =
      BuildThirdOpacity(link_color);

  VerifyColorMap(colors);
}

TEST_F(BrowserThemePackTest, ProvideUnderlineLinkColor) {
  // If we specify the underline color, it shouldn't try to generate one.
  std::string color_json = "{ \"ntp_link\": [128, 128, 128],"
                           "  \"ntp_link_underline\": [255, 255, 255],"
                           "  \"ntp_section_link\": [128, 128, 128],"
                           "  \"ntp_section_link_underline\": [255, 255, 255]"
                           "}";
  LoadColorJSON(color_json);

  std::map<int, SkColor> colors = GetDefaultColorMap();
  SkColor link_color = SkColorSetRGB(128, 128, 128);
  SkColor underline_color = SkColorSetRGB(255, 255, 255);
  colors[ThemeProperties::COLOR_NTP_LINK] = link_color;
  colors[ThemeProperties::COLOR_NTP_LINK_UNDERLINE] = underline_color;
  colors[ThemeProperties::COLOR_NTP_SECTION_LINK] = link_color;
  colors[ThemeProperties::COLOR_NTP_SECTION_LINK_UNDERLINE] =
      underline_color;

  VerifyColorMap(colors);
}

TEST_F(BrowserThemePackTest, UseSectionColorAsNTPHeader) {
  std::string color_json = "{ \"ntp_section\": [190, 190, 190] }";
  LoadColorJSON(color_json);

  std::map<int, SkColor> colors = GetDefaultColorMap();
  SkColor ntp_color = SkColorSetRGB(190, 190, 190);
  colors[ThemeProperties::COLOR_NTP_HEADER] = ntp_color;
  colors[ThemeProperties::COLOR_NTP_SECTION] = ntp_color;
  VerifyColorMap(colors);
}

TEST_F(BrowserThemePackTest, ProvideNtpHeaderColor) {
  std::string color_json = "{ \"ntp_header\": [120, 120, 120], "
                           "  \"ntp_section\": [190, 190, 190] }";
  LoadColorJSON(color_json);

  std::map<int, SkColor> colors = GetDefaultColorMap();
  SkColor ntp_header = SkColorSetRGB(120, 120, 120);
  SkColor ntp_section = SkColorSetRGB(190, 190, 190);
  colors[ThemeProperties::COLOR_NTP_HEADER] = ntp_header;
  colors[ThemeProperties::COLOR_NTP_SECTION] = ntp_section;
  VerifyColorMap(colors);
}

TEST_F(BrowserThemePackTest, CanReadTints) {
  std::string tint_json = "{ \"buttons\": [ 0.5, 0.5, 0.5 ] }";
  LoadTintJSON(tint_json);

  color_utils::HSL expected = { 0.5, 0.5, 0.5 };
  color_utils::HSL actual = { -1, -1, -1 };
  EXPECT_TRUE(theme_pack_->GetTint(
      ThemeProperties::TINT_BUTTONS, &actual));
  EXPECT_DOUBLE_EQ(expected.h, actual.h);
  EXPECT_DOUBLE_EQ(expected.s, actual.s);
  EXPECT_DOUBLE_EQ(expected.l, actual.l);
}

TEST_F(BrowserThemePackTest, CanReadDisplayProperties) {
  std::string json = "{ \"ntp_background_alignment\": \"bottom\", "
                     "  \"ntp_background_repeat\": \"repeat-x\", "
                     "  \"ntp_logo_alternate\": 0 }";
  LoadDisplayPropertiesJSON(json);

  int out_val;
  EXPECT_TRUE(theme_pack_->GetDisplayProperty(
      ThemeProperties::NTP_BACKGROUND_ALIGNMENT, &out_val));
  EXPECT_EQ(ThemeProperties::ALIGN_BOTTOM, out_val);

  EXPECT_TRUE(theme_pack_->GetDisplayProperty(
      ThemeProperties::NTP_BACKGROUND_TILING, &out_val));
  EXPECT_EQ(ThemeProperties::REPEAT_X, out_val);

  EXPECT_TRUE(theme_pack_->GetDisplayProperty(
      ThemeProperties::NTP_LOGO_ALTERNATE, &out_val));
  EXPECT_EQ(0, out_val);
}

TEST_F(BrowserThemePackTest, CanParsePaths) {
  std::string path_json = "{ \"theme_button_background\": \"one\", "
                          "  \"theme_toolbar\": \"two\" }";
  std::map<int, base::FilePath> out_file_paths;
  ParseImageNamesJSON(path_json, &out_file_paths);

  size_t expected_file_paths = 2u;
#if defined(OS_WIN) && defined(USE_AURA)
  // On Windows AURA additional theme paths are generated to map to the special
  // resource ids like IDR_THEME_FRAME_WIN, etc
  expected_file_paths = 3u;
#endif
  EXPECT_EQ(expected_file_paths, out_file_paths.size());
  // "12" and "5" are internal constants to BrowserThemePack and are
  // PRS_THEME_BUTTON_BACKGROUND and PRS_THEME_TOOLBAR, but they are
  // implementation details that shouldn't be exported.
  EXPECT_TRUE(base::FilePath(FILE_PATH_LITERAL("one")) == out_file_paths[12]);
  EXPECT_TRUE(base::FilePath(FILE_PATH_LITERAL("two")) == out_file_paths[5]);
}

TEST_F(BrowserThemePackTest, InvalidPathNames) {
  std::string path_json = "{ \"wrong\": [1], "
                          "  \"theme_button_background\": \"one\", "
                          "  \"not_a_thing\": \"blah\" }";
  std::map<int, base::FilePath> out_file_paths;
  ParseImageNamesJSON(path_json, &out_file_paths);

  // We should have only parsed one valid path out of that mess above.
  EXPECT_EQ(1u, out_file_paths.size());
}

TEST_F(BrowserThemePackTest, InvalidColors) {
  std::string invalid_color = "{ \"toolbar\": [\"dog\", \"cat\", [12]], "
                              "  \"sound\": \"woof\" }";
  LoadColorJSON(invalid_color);
  std::map<int, SkColor> colors = GetDefaultColorMap();
  VerifyColorMap(colors);
}

TEST_F(BrowserThemePackTest, InvalidTints) {
  std::string invalid_tints = "{ \"buttons\": [ \"dog\", \"cat\", [\"x\"]], "
                              "  \"invalid\": \"entry\" }";
  LoadTintJSON(invalid_tints);

  // We shouldn't have a buttons tint, as it was invalid.
  color_utils::HSL actual = { -1, -1, -1 };
  EXPECT_FALSE(theme_pack_->GetTint(ThemeProperties::TINT_BUTTONS,
                                    &actual));
}

TEST_F(BrowserThemePackTest, InvalidDisplayProperties) {
  std::string invalid_properties = "{ \"ntp_background_alignment\": [15], "
                                   "  \"junk\": [15.3] }";
  LoadDisplayPropertiesJSON(invalid_properties);

  int out_val;
  EXPECT_FALSE(theme_pack_->GetDisplayProperty(
      ThemeProperties::NTP_BACKGROUND_ALIGNMENT, &out_val));
}

// These three tests should just not cause a segmentation fault.
TEST_F(BrowserThemePackTest, NullPaths) {
  std::map<int, base::FilePath> out_file_paths;
  ParseImageNamesDictionary(NULL, &out_file_paths);
}

TEST_F(BrowserThemePackTest, NullTints) {
  LoadTintDictionary(NULL);
}

TEST_F(BrowserThemePackTest, NullColors) {
  LoadColorDictionary(NULL);
}

TEST_F(BrowserThemePackTest, NullDisplayProperties) {
  LoadDisplayPropertiesDictionary(NULL);
}

TEST_F(BrowserThemePackTest, TestHasCustomImage) {
  // HasCustomImage should only return true for images that exist in the
  // extension and not for autogenerated images.
  std::string images = "{ \"theme_frame\": \"one\" }";
  std::map<int, base::FilePath> out_file_paths;
  ParseImageNamesJSON(images, &out_file_paths);

  EXPECT_TRUE(theme_pack_->HasCustomImage(IDR_THEME_FRAME));
  EXPECT_FALSE(theme_pack_->HasCustomImage(IDR_THEME_FRAME_INCOGNITO));
}

TEST_F(BrowserThemePackTest, TestNonExistantImages) {
  std::string images = "{ \"theme_frame\": \"does_not_exist\" }";
  std::map<int, base::FilePath> out_file_paths;
  ParseImageNamesJSON(images, &out_file_paths);

  EXPECT_FALSE(LoadRawBitmapsTo(out_file_paths));
}

// TODO(erg): This test should actually test more of the built resources from
// the extension data, but for now, exists so valgrind can test some of the
// tricky memory stuff that BrowserThemePack does.
TEST_F(BrowserThemePackTest, CanBuildAndReadPack) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath file = dir.path().AppendASCII("data.pak");

  // Part 1: Build the pack from an extension.
  {
    base::FilePath star_gazing_path = GetStarGazingPath();
    base::FilePath manifest_path =
        star_gazing_path.AppendASCII("manifest.json");
    std::string error;
    JSONFileValueSerializer serializer(manifest_path);
    scoped_ptr<DictionaryValue> valid_value(
        static_cast<DictionaryValue*>(serializer.Deserialize(NULL, &error)));
    EXPECT_EQ("", error);
    ASSERT_TRUE(valid_value.get());
    scoped_refptr<Extension> extension(Extension::Create(
        star_gazing_path, extensions::Manifest::INVALID_LOCATION, *valid_value,
        Extension::REQUIRE_KEY, &error));
    ASSERT_TRUE(extension.get());
    ASSERT_EQ("", error);

    scoped_refptr<BrowserThemePack> pack(
        BrowserThemePack::BuildFromExtension(extension.get()));
    ASSERT_TRUE(pack.get());
    ASSERT_TRUE(pack->WriteToDisk(file));
    VerifyStarGazing(pack.get());
  }

  // Part 2: Try to read back the data pack that we just wrote to disk.
  {
    scoped_refptr<BrowserThemePack> pack =
        BrowserThemePack::BuildFromDataPack(
            file, "mblmlcbknbnfebdfjnolmcapmdofhmme");
    ASSERT_TRUE(pack.get());
    VerifyStarGazing(pack.get());
  }
}
