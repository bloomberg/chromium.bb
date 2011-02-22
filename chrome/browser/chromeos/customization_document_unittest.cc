// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGoodStartupManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"initial_locale\" : \"en-US\","
    "  \"initial_timezone\" : \"US/Pacific\","
    "  \"registration_url\" : \"http://www.google.com\","
    "  \"setup_content\" : {"
    "    \"en-US\" : {"
    "      \"help_page\" : \"file:///opt/oem/help/en-US/help.html\","
    "      \"eula_page\" : \"file:///opt/oem/eula/en-US/eula.html\","
    "    },"
    "    \"ru-RU\" : {"
    "      \"help_page\" : \"file:///opt/oem/help/ru-RU/help.html\","
    "      \"eula_page\" : \"file:///opt/oem/eula/ru-RU/eula.html\","
    "    },"
    "    \"default\" : {"
    "      \"help_page\" : \"file:///opt/oem/help/en/help.html\","
    "      \"eula_page\" : \"file:///opt/oem/eula/en/eula.html\","
    "    },"
    "  },"
    "  \"hwidmap\" : {"
    "    \"Mario\" : {"
    "      \"initial_locale\" : \"ru-RU\","
    "      \"initial_timezone\" : \"Europe/Moscow\","
    "    },"
    "    \"ZGA\" : {"
    "      \"initial_locale\" : \"ja\","
    "      \"initial_timezone\" : \"Asia/Tokyo\","
    "    },"
    "  },"
    "}";

const char kBadManifest[] = "{\"version\": \"1\"}";

const char kGoodServicesManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"app_content\" : {"
    "    \"en-US\" : {"
    "      \"initial_start_page\": \"http://mario/promo\","
    "      \"support_page\": \"http://mario/us\","
    "    },"
    "    \"ru-RU\" : {"
    "      \"initial_start_page\": \"http://mario/ru/promo\","
    "      \"support_page\": \"http://mario/ru\","
    "    },"
    "    \"default\" : {"
    "      \"initial_start_page\": \"http://mario/global/promo\","
    "      \"support_page\": \"http://mario/global\","
    "    },"
    "  },"
    "}";

class TestDocument : public chromeos::StartupCustomizationDocument {
 private:
  virtual std::string GetHWID() const {
    return "Mario";
  }
};

}  // anonymous namespace

// StartupCustomizationDocumentTest implementation.
class StartupCustomizationDocumentTest : public testing::Test {
 protected:
  TestDocument customization_;
};

TEST_F(StartupCustomizationDocumentTest, Basic) {
  EXPECT_TRUE(customization_.LoadManifestFromString(kGoodStartupManifest));
  EXPECT_EQ(customization_.initial_locale(), "ru-RU");
  EXPECT_EQ(customization_.initial_timezone(), "Europe/Moscow");
  EXPECT_EQ(customization_.registration_url(), "http://www.google.com");

  EXPECT_EQ(customization_.GetHelpPage("en-US"),
            "file:///opt/oem/help/en-US/help.html");
  EXPECT_EQ(customization_.GetHelpPage("ru-RU"),
            "file:///opt/oem/help/ru-RU/help.html");
  EXPECT_EQ(customization_.GetHelpPage("ja"),
            "file:///opt/oem/help/en/help.html");

  EXPECT_EQ(customization_.GetEULAPage("en-US"),
            "file:///opt/oem/eula/en-US/eula.html");
  EXPECT_EQ(customization_.GetEULAPage("ru-RU"),
            "file:///opt/oem/eula/ru-RU/eula.html");
  EXPECT_EQ(customization_.GetEULAPage("jp-JP"),
            "file:///opt/oem/eula/en/eula.html");
}

TEST_F(StartupCustomizationDocumentTest, BadManifest) {
  EXPECT_FALSE(customization_.LoadManifestFromString(kBadManifest));
}

// ServicesCustomizationDocumentTest implementation.
class ServicesCustomizationDocumentTest : public testing::Test {
 protected:
  chromeos::ServicesCustomizationDocument customization_;
};

TEST_F(ServicesCustomizationDocumentTest, Basic) {
  EXPECT_TRUE(customization_.LoadManifestFromString(kGoodServicesManifest));

  EXPECT_EQ(customization_.GetInitialStartPage("en-US"),
            "http://mario/promo");
  EXPECT_EQ(customization_.GetInitialStartPage("ru-RU"),
            "http://mario/ru/promo");
  EXPECT_EQ(customization_.GetInitialStartPage("jp-JP"),
            "http://mario/global/promo");


  EXPECT_EQ(customization_.GetSupportPage("en-US"),
            "http://mario/us");
  EXPECT_EQ(customization_.GetSupportPage("ru-RU"),
            "http://mario/ru");
  EXPECT_EQ(customization_.GetSupportPage("jp-JP"),
            "http://mario/global");
}

TEST_F(ServicesCustomizationDocumentTest, BadManifest) {
  EXPECT_FALSE(customization_.LoadManifestFromString(kBadManifest));
}
