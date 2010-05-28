// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGoodStartupManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"product_sku\" : \"SKU\","
    "  \"initial_locale\" : \"en_US\","
    "  \"background_color\" : \"#880088\","
    "  \"registration_url\" : \"http://www.google.com\","
    "  \"setup_content\" : ["
    "    {"
    "      \"content_locale\" : \"en_US\","
    "      \"help_page\" : \"setup_content/en_US/help.html\","
    "      \"eula_page\" : \"setup_content/en_US/eula.html\","
    "    },"
    "    {"
    "      \"content_locale\" : \"ru\","
    "      \"help_page\" : \"setup_content/ru/help.html\","
    "      \"eula_page\" : \"setup_content/ru/eula.html\","
    "    },"
    "  ]"
    "}";

const char kBadStartupManifest1[] = "{}";
const char kBadStartupManifest2[] = "{ \"version\" : \"1.0\" }";
const char kBadStartupManifest3[] = "{"
    "  \"version\" : \"0.0\","
    "  \"product_sku\" : \"SKU\","
    "}";

const char kBadStartupManifest4[] = "{"
    "  \"version\" : \"1.0\","
    "  \"product_sku\" : \"SKU\","
    "  \"setup_content\" : ["
    "    {"
    "      \"help_page\" : \"setup_content/en_US/help.html\","
    "      \"eula_page\" : \"setup_content/en_US/eula.html\","
    "    },"
    "  ]"
    "}";

const char kBadStartupManifest5[] = "{"
    "  \"version\" : \"1.0\","
    "  \"product_sku\" : \"SKU\","
    "  \"setup_content\" : ["
    "    {"
    "      \"content_locale\" : \"en_US\","
    "      \"eula_page\" : \"setup_content/en_US/eula.html\","
    "    },"
    "  ]"
    "}";



}  // anonymous namespace

class StartupCustomizationDocumentTest : public testing::Test {
 public:
  chromeos::StartupCustomizationDocument customization_;
};

TEST_F(StartupCustomizationDocumentTest, LoadBadStartupManifestFromString) {
  bool result = false;
  result = customization_.LoadManifestFromString(kBadStartupManifest1);
  EXPECT_EQ(result, false);
  result = customization_.LoadManifestFromString(kBadStartupManifest2);
  EXPECT_EQ(result, false);
  result = customization_.LoadManifestFromString(kBadStartupManifest3);
  EXPECT_EQ(result, false);
  result = customization_.LoadManifestFromString(kBadStartupManifest4);
  EXPECT_EQ(result, false);
  result = customization_.LoadManifestFromString(kBadStartupManifest5);
  EXPECT_EQ(result, false);
}

TEST_F(StartupCustomizationDocumentTest, LoadGoodStartupManifestFromString) {
  bool result = false;
  result = customization_.LoadManifestFromString(kGoodStartupManifest);
  EXPECT_EQ(result, true);
  EXPECT_EQ(customization_.version(), "1.0");
  EXPECT_EQ(customization_.product_sku(), "SKU");
  EXPECT_EQ(customization_.initial_locale(), "en_US");
  EXPECT_EQ(customization_.background_color(),
            SkColorSetRGB(0x88, 0x00, 0x88));
  EXPECT_EQ(customization_.registration_url(), "http://www.google.com");

  EXPECT_EQ(customization_.GetSetupContent("en_US")->help_page_path,
            "setup_content/en_US/help.html");
  EXPECT_EQ(customization_.GetSetupContent("en_US")->eula_page_path,
            "setup_content/en_US/eula.html");
  EXPECT_EQ(customization_.GetSetupContent("ru")->help_page_path,
            "setup_content/ru/help.html");
  EXPECT_EQ(customization_.GetSetupContent("ru")->eula_page_path,
            "setup_content/ru/eula.html");
}
