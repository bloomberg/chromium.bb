// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font/font_loader_test.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/fontconfig_util_linux.h"
#include "components/services/font/public/interfaces/constants.mojom.h"
#include "components/services/font/public/interfaces/font_service.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkFontStyle.h"

namespace {
bool IsInTestFontDirectory(const char* path) {
  const char kTestFontsDir[] = "test_fonts";
  return std::string(path).find(kTestFontsDir) != std::string::npos;
}
}  // namespace

namespace font_service {

FontLoaderTest::FontLoaderTest() : ServiceTest("font_service_unittests") {}

FontLoaderTest::~FontLoaderTest() {}

void FontLoaderTest::SetUp() {
  ServiceTest::SetUp();
  font_loader_ = std::make_unique<FontLoader>(connector());
}

TEST_F(FontLoaderTest, BasicMatchingTest) {
  SkFontStyle styles[] = {
      SkFontStyle(SkFontStyle::kNormal_Weight, SkFontStyle::kNormal_Width,
                  SkFontStyle::kUpright_Slant),
      SkFontStyle(SkFontStyle::kBold_Weight, SkFontStyle::kNormal_Width,
                  SkFontStyle::kUpright_Slant),
      SkFontStyle(SkFontStyle::kBold_Weight, SkFontStyle::kNormal_Width,
                  SkFontStyle::kItalic_Slant)};
  // See kFontsConfTemplate[] in fontconfig_util_linux.cc for details of which
  // fonts can be picked. Arial, Times New Roman and Courier New are aliased to
  // Arimos, Tinos and Cousine ChromeOS open source alternatives when FontConfig
  // is set up for testing.
  std::vector<std::vector<std::string>> family_names_expected_names = {
      {"Arial", "Arimo"},
      {"Times New Roman", "Tinos"},
      {"Courier New", "Cousine"}};
  for (std::vector<std::string> request_family_name :
       family_names_expected_names) {
    for (auto& request_style : styles) {
      SkFontConfigInterface::FontIdentity font_identity;
      SkFontStyle result_style;
      SkString result_family_name;
      font_loader()->matchFamilyName(request_family_name[0].c_str(),
                                     request_style, &font_identity,
                                     &result_family_name, &result_style);
      EXPECT_EQ(request_family_name[1],
                std::string(result_family_name.c_str()));
      EXPECT_TRUE(IsInTestFontDirectory(font_identity.fString.c_str()));
      EXPECT_EQ(result_style, request_style);
    }
  }
}

TEST_F(FontLoaderTest, NotFoundTest) {
  std::string request_family_name = {"IMPROBABLE_FONT_NAME"};
  SkFontConfigInterface::FontIdentity font_identity;
  SkFontStyle result_style;
  SkString result_family_name;
  font_loader()->matchFamilyName(request_family_name.c_str(), SkFontStyle(),
                                 &font_identity, &result_family_name,
                                 &result_style);
  EXPECT_EQ("", std::string(result_family_name.c_str()));
  EXPECT_EQ(0u, font_identity.fID);
  EXPECT_EQ("", std::string(font_identity.fString.c_str()));
}

TEST_F(FontLoaderTest, EmptyFontName) {
  std::string request_family_name = {""};
  std::string kDefaultFontName = "DejaVu Sans";
  SkFontConfigInterface::FontIdentity font_identity;
  SkFontStyle result_style;
  SkString result_family_name;
  font_loader()->matchFamilyName(request_family_name.c_str(), SkFontStyle(),
                                 &font_identity, &result_family_name,
                                 &result_style);
  EXPECT_EQ(kDefaultFontName, std::string(result_family_name.c_str()));
  EXPECT_TRUE(IsInTestFontDirectory(font_identity.fString.c_str()));
}

}  // namespace font_service
