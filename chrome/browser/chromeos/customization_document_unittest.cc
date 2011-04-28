// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

#include "chrome/browser/chromeos/mock_system_access.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGoodStartupManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"initial_locale\" : \"en-US\","
    "  \"initial_timezone\" : \"US/Pacific\","
    "  \"keyboard_layout\" : \"xkb:us::eng\","
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
    "  \"hwid_map\" : ["
    "    {"
    "      \"hwid_mask\": \"ZGA*34\","
    "      \"initial_locale\" : \"ja\","
    "      \"initial_timezone\" : \"Asia/Tokyo\","
    "      \"keyboard_layout\" : \"mozc-jp\","
    "    },"
    "    {"
    "      \"hwid_mask\": \"Mario 1?3*\","
    "      \"initial_locale\" : \"ru-RU\","
    "      \"initial_timezone\" : \"Europe/Moscow\","
    "      \"keyboard_layout\" : \"xkb:ru::rus\","
    "    },"
    "  ],"
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

}  // anonymous namespace

namespace chromeos {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;

TEST(StartupCustomizationDocumentTest, Basic) {
  MockSystemAccess mock_system_access;
  EXPECT_CALL(mock_system_access, GetMachineStatistic(_, NotNull()))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_system_access,
      GetMachineStatistic(std::string("hwid"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("Mario 12345")),
                          Return(true)));
  StartupCustomizationDocument customization(&mock_system_access,
                                             kGoodStartupManifest);
  EXPECT_EQ(customization.initial_locale(), "ru-RU");
  EXPECT_EQ(customization.initial_timezone(), "Europe/Moscow");
  EXPECT_EQ(customization.keyboard_layout(), "xkb:ru::rus");
  EXPECT_EQ(customization.registration_url(), "http://www.google.com");

  EXPECT_EQ(customization.GetHelpPage("en-US"),
            "file:///opt/oem/help/en-US/help.html");
  EXPECT_EQ(customization.GetHelpPage("ru-RU"),
            "file:///opt/oem/help/ru-RU/help.html");
  EXPECT_EQ(customization.GetHelpPage("ja"),
            "file:///opt/oem/help/en/help.html");

  EXPECT_EQ(customization.GetEULAPage("en-US"),
            "file:///opt/oem/eula/en-US/eula.html");
  EXPECT_EQ(customization.GetEULAPage("ru-RU"),
            "file:///opt/oem/eula/ru-RU/eula.html");
  EXPECT_EQ(customization.GetEULAPage("ja"),
            "file:///opt/oem/eula/en/eula.html");
}

TEST(StartupCustomizationDocumentTest, VPD) {
  MockSystemAccess mock_system_access;
  EXPECT_CALL(mock_system_access,
      GetMachineStatistic(std::string("hwid"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("Mario 12345")),
                          Return(true)));
  EXPECT_CALL(mock_system_access,
      GetMachineStatistic(std::string("initial_locale"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("ja")),
                          Return(true)));
  EXPECT_CALL(mock_system_access,
      GetMachineStatistic(std::string("initial_timezone"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("Asia/Tokyo")),
                          Return(true)));
  EXPECT_CALL(mock_system_access,
      GetMachineStatistic(std::string("keyboard_layout"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("mozc-jp")),
                          Return(true)));
  StartupCustomizationDocument customization(&mock_system_access,
                                             kGoodStartupManifest);
  EXPECT_TRUE(customization.IsReady());
  EXPECT_EQ(customization.initial_locale(), "ja");
  EXPECT_EQ(customization.initial_timezone(), "Asia/Tokyo");
  EXPECT_EQ(customization.keyboard_layout(), "mozc-jp");
}

TEST(StartupCustomizationDocumentTest, BadManifest) {
  MockSystemAccess mock_system_access;
  StartupCustomizationDocument customization(&mock_system_access, kBadManifest);
  EXPECT_FALSE(customization.IsReady());
}

TEST(ServicesCustomizationDocumentTest, Basic) {
  ServicesCustomizationDocument customization(kGoodServicesManifest);
  EXPECT_TRUE(customization.IsReady());

  EXPECT_EQ(customization.GetInitialStartPage("en-US"),
            "http://mario/promo");
  EXPECT_EQ(customization.GetInitialStartPage("ru-RU"),
            "http://mario/ru/promo");
  EXPECT_EQ(customization.GetInitialStartPage("ja"),
            "http://mario/global/promo");

  EXPECT_EQ(customization.GetSupportPage("en-US"),
            "http://mario/us");
  EXPECT_EQ(customization.GetSupportPage("ru-RU"),
            "http://mario/ru");
  EXPECT_EQ(customization.GetSupportPage("ja"),
            "http://mario/global");
}

TEST(ServicesCustomizationDocumentTest, BadManifest) {
  ServicesCustomizationDocument customization(kBadManifest);
  EXPECT_FALSE(customization.IsReady());
}

}  // namespace chromeos
