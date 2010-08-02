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
    "  \"initial_timezone\" : \"US/Pacific\","
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

const char kGoodServicesManifest[] = "{"
    "  // Required.\n"
    "  \"version\": \"1.0\","
    "  \"app_menu\" : {"
    "    \"section_title\" : \"App menu title.\","
    "    \"web_apps\" : ["
    "      \"http://localhost/a/1\","
    "      \"http://localhost/a/2\","
    "    ],"
    "    \"support_page\": \"http://localhost/h\","
    "    \"extensions\": ["
    "      \"http://localhost/e/1\","
    "      \"http://localhost/e/2\","
    "    ],"
    "  },"
    " // Optional.\n"
    "  \"initial_start_page\": \"http://localhost/s\","
    "}";

}  // anonymous namespace

// StartupCustomizationDocumentTest implementation.

class StartupCustomizationDocumentTest : public testing::Test {
 protected:
  chromeos::StartupCustomizationDocument customization_;
};

TEST_F(StartupCustomizationDocumentTest, LoadBadManifestFromString) {
  EXPECT_FALSE(customization_.LoadManifestFromString(kBadStartupManifest1));
  EXPECT_FALSE(customization_.LoadManifestFromString(kBadStartupManifest2));
  EXPECT_FALSE(customization_.LoadManifestFromString(kBadStartupManifest3));
  EXPECT_FALSE(customization_.LoadManifestFromString(kBadStartupManifest4));
  EXPECT_FALSE(customization_.LoadManifestFromString(kBadStartupManifest5));
}

TEST_F(StartupCustomizationDocumentTest, LoadGoodManifestFromString) {
  EXPECT_TRUE(customization_.LoadManifestFromString(kGoodStartupManifest));
  EXPECT_EQ(customization_.version(), "1.0");
  EXPECT_EQ(customization_.product_sku(), "SKU");
  EXPECT_EQ(customization_.initial_locale(), "en_US");
  EXPECT_EQ(customization_.initial_timezone(), "US/Pacific");
  EXPECT_EQ(customization_.background_color(),
            SkColorSetRGB(0x88, 0x00, 0x88));
  EXPECT_EQ(customization_.registration_url(), "http://www.google.com");
  EXPECT_EQ(customization_.GetHelpPagePath("en_US").value(),
            "setup_content/en_US/help.html");
  EXPECT_EQ(customization_.GetEULAPagePath("en_US").value(),
            "setup_content/en_US/eula.html");
  EXPECT_EQ(customization_.GetHelpPagePath("ru").value(),
            "setup_content/ru/help.html");
  EXPECT_EQ(customization_.GetEULAPagePath("ru").value(),
            "setup_content/ru/eula.html");
}

// ServicesCustomizationDocumentTest implementation.

class ServicesCustomizationDocumentTest : public testing::Test {
 protected:
  chromeos::ServicesCustomizationDocument customization_;
};

TEST_F(ServicesCustomizationDocumentTest, LoadBadManifestFromString) {
  EXPECT_FALSE(customization_.LoadManifestFromString(kBadStartupManifest1));
  EXPECT_FALSE(customization_.LoadManifestFromString(kBadStartupManifest2));
}

TEST_F(ServicesCustomizationDocumentTest, LoadGoodManifestFromString) {
  EXPECT_TRUE(customization_.LoadManifestFromString(kGoodServicesManifest));
  EXPECT_EQ(customization_.version(), "1.0");
  EXPECT_EQ(customization_.app_menu_section_title(), "App menu title.");
  EXPECT_EQ(customization_.app_menu_support_page_url(), "http://localhost/h");
  EXPECT_EQ(customization_.initial_start_page_url(), "http://localhost/s");
  EXPECT_EQ(customization_.web_apps().front(), "http://localhost/a/1");
  EXPECT_EQ(customization_.web_apps().back(), "http://localhost/a/2");
  EXPECT_EQ(customization_.extensions().front(), "http://localhost/e/1");
  EXPECT_EQ(customization_.extensions().back(), "http://localhost/e/2");
}
