// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

#include "chrome/browser/chromeos/system/mock_statistics_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  system::MockStatisticsProvider mock_statistics_provider;
  EXPECT_CALL(mock_statistics_provider, GetMachineStatistic(_, NotNull()))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("hardware_class"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("Mario 12345")),
                          Return(true)));
  StartupCustomizationDocument customization(&mock_statistics_provider,
                                             kGoodStartupManifest);
  EXPECT_EQ("ru-RU", customization.initial_locale());
  EXPECT_EQ("Europe/Moscow", customization.initial_timezone());
  EXPECT_EQ("xkb:ru::rus", customization.keyboard_layout());
  EXPECT_EQ("http://www.google.com", customization.registration_url());

  EXPECT_EQ("file:///opt/oem/help/en-US/help.html",
            customization.GetHelpPage("en-US"));
  EXPECT_EQ("file:///opt/oem/help/ru-RU/help.html",
            customization.GetHelpPage("ru-RU"));
  EXPECT_EQ("file:///opt/oem/help/en/help.html",
            customization.GetHelpPage("ja"));

  EXPECT_EQ("file:///opt/oem/eula/en-US/eula.html",
            customization.GetEULAPage("en-US"));
  EXPECT_EQ("file:///opt/oem/eula/ru-RU/eula.html",
            customization.GetEULAPage("ru-RU"));
  EXPECT_EQ("file:///opt/oem/eula/en/eula.html",
            customization.GetEULAPage("ja"));
}

TEST(StartupCustomizationDocumentTest, VPD) {
  system::MockStatisticsProvider mock_statistics_provider;
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("hardware_class"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("Mario 12345")),
                          Return(true)));
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("initial_locale"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("ja")),
                          Return(true)));
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("initial_timezone"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("Asia/Tokyo")),
                          Return(true)));
  EXPECT_CALL(mock_statistics_provider,
      GetMachineStatistic(std::string("keyboard_layout"), NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(std::string("mozc-jp")),
                          Return(true)));
  StartupCustomizationDocument customization(&mock_statistics_provider,
                                             kGoodStartupManifest);
  EXPECT_TRUE(customization.IsReady());
  EXPECT_EQ("ja", customization.initial_locale());
  EXPECT_EQ("Asia/Tokyo", customization.initial_timezone());
  EXPECT_EQ("mozc-jp", customization.keyboard_layout());
}

TEST(StartupCustomizationDocumentTest, BadManifest) {
  system::MockStatisticsProvider mock_statistics_provider;
  StartupCustomizationDocument customization(&mock_statistics_provider,
                                             kBadManifest);
  EXPECT_FALSE(customization.IsReady());
}

TEST(ServicesCustomizationDocumentTest, Basic) {
  ServicesCustomizationDocument customization(kGoodServicesManifest);
  EXPECT_TRUE(customization.IsReady());

  EXPECT_EQ("http://mario/promo",
            customization.GetInitialStartPage("en-US"));
  EXPECT_EQ("http://mario/ru/promo",
            customization.GetInitialStartPage("ru-RU"));
  EXPECT_EQ("http://mario/global/promo",
            customization.GetInitialStartPage("ja"));

  EXPECT_EQ("http://mario/us", customization.GetSupportPage("en-US"));
  EXPECT_EQ("http://mario/ru", customization.GetSupportPage("ru-RU"));
  EXPECT_EQ("http://mario/global", customization.GetSupportPage("ja"));
}

TEST(ServicesCustomizationDocumentTest, BadManifest) {
  ServicesCustomizationDocument customization(kBadManifest);
  EXPECT_FALSE(customization.IsReady());
}

}  // namespace chromeos
