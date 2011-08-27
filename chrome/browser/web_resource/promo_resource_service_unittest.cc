// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/apps_promo.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class PromoResourceServiceTest : public testing::Test {
 public:
  PromoResourceServiceTest()
      : local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)),
        web_resource_service_(new PromoResourceService(&profile_)) {
  }

 protected:
  TestingProfile profile_;
  ScopedTestingLocalState local_state_;
  scoped_refptr<PromoResourceService> web_resource_service_;
};

// Verifies that custom dates read from a web resource server are written to
// the preferences file.
TEST_F(PromoResourceServiceTest, UnpackLogoSignal) {
  // Set up start and end dates in a Dictionary as if parsed from the service.
  std::string json = "{ "
                     "  \"topic\": {"
                     "    \"answers\": ["
                     "       {"
                     "        \"name\": \"custom_logo_start\","
                     "        \"inproduct\": \"31/01/10 01:00 GMT\""
                     "       },"
                     "       {"
                     "        \"name\": \"custom_logo_end\","
                     "        \"inproduct\": \"31/01/12 01:00 GMT\""
                     "       }"
                     "    ]"
                     "  }"
                     "}";
  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackLogoSignal(*(test_json.get()));
  PrefService* prefs = profile_.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  double logo_start =
      prefs->GetDouble(prefs::kNTPCustomLogoStart);
  EXPECT_EQ(logo_start, 1264899600);  // unix epoch for Jan 31 2010 0100 GMT.
  double logo_end =
      prefs->GetDouble(prefs::kNTPCustomLogoEnd);
  EXPECT_EQ(logo_end, 1327971600);  // unix epoch for Jan 31 2012 0100 GMT.

  // Change the start only and recheck.
  json = "{ "
         "  \"topic\": {"
         "    \"answers\": ["
         "       {"
         "        \"name\": \"custom_logo_start\","
         "        \"inproduct\": \"28/02/10 14:00 GMT\""
         "       },"
         "       {"
         "        \"name\": \"custom_logo_end\","
         "        \"inproduct\": \"31/01/12 01:00 GMT\""
         "       }"
         "    ]"
         "  }"
         "}";
  test_json->Clear();
  test_json.reset(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackLogoSignal(*(test_json.get()));

  logo_start = prefs->GetDouble(prefs::kNTPCustomLogoStart);
  EXPECT_EQ(logo_start, 1267365600);  // date changes to Feb 28 2010 1400 GMT.

  // If no date is included in the prefs, reset custom logo dates to 0.
  json = "{ "
         "  \"topic\": {"
         "    \"answers\": ["
         "       {"
         "       }"
         "    ]"
         "  }"
         "}";
  test_json->Clear();
  test_json.reset(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackLogoSignal(*(test_json.get()));
  logo_start = prefs->GetDouble(prefs::kNTPCustomLogoStart);
  EXPECT_EQ(logo_start, 0);  // date value reset to 0;
  logo_end = prefs->GetDouble(prefs::kNTPCustomLogoEnd);
  EXPECT_EQ(logo_end, 0);  // date value reset to 0;
}

TEST_F(PromoResourceServiceTest, UnpackPromoSignal) {
  // Set up start and end dates and promo line in a Dictionary as if parsed
  // from the service.
  std::string json = "{ "
                     "  \"topic\": {"
                     "    \"answers\": ["
                     "       {"
                     "        \"name\": \"promo_start\","
                     "        \"question\": \"3:2\","
                     "        \"tooltip\": \"Eat more pie!\","
                     "        \"inproduct\": \"31/01/10 01:00 GMT\""
                     "       },"
                     "       {"
                     "        \"name\": \"promo_end\","
                     "        \"inproduct\": \"31/01/12 01:00 GMT\""
                     "       }"
                     "    ]"
                     "  }"
                     "}";
  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Initialize a message loop for this to run on.
  MessageLoop loop;

  // Check that prefs are set correctly.
  web_resource_service_->UnpackPromoSignal(*(test_json.get()));
  PrefService* prefs = profile_.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  std::string promo_line = prefs->GetString(prefs::kNTPPromoLine);
  EXPECT_EQ(promo_line, "Eat more pie!");

  int promo_group = prefs->GetInteger(prefs::kNTPPromoGroup);
  EXPECT_GE(promo_group, 0);
  EXPECT_LT(promo_group, 16);

  int promo_build_type = prefs->GetInteger(prefs::kNTPPromoBuild);
  EXPECT_EQ(promo_build_type & PromoResourceService::DEV_BUILD,
            PromoResourceService::DEV_BUILD);
  EXPECT_EQ(promo_build_type & PromoResourceService::BETA_BUILD,
            PromoResourceService::BETA_BUILD);
  EXPECT_EQ(promo_build_type & PromoResourceService::STABLE_BUILD, 0);

  int promo_time_slice = prefs->GetInteger(prefs::kNTPPromoGroupTimeSlice);
  EXPECT_EQ(promo_time_slice, 2);

  double promo_start =
      prefs->GetDouble(prefs::kNTPPromoStart);
  int64 actual_start = 1264899600 +  // unix epoch for Jan 31 2010 0100 GMT.
      promo_group * 2 * 60 * 60;
  EXPECT_EQ(promo_start, actual_start);

  double promo_end =
      prefs->GetDouble(prefs::kNTPPromoEnd);
  EXPECT_EQ(promo_end, 1327971600);  // unix epoch for Jan 31 2012 0100 GMT.
}

TEST_F(PromoResourceServiceTest, UnpackWebStoreSignal) {
  web_resource_service_->set_channel(chrome::VersionInfo::CHANNEL_DEV);

  std::string json = "{ "
                     "  \"topic\": {"
                     "    \"answers\": ["
                     "       {"
                     "        \"answer_id\": \"341252\","
                     "        \"name\": \"webstore_promo:15:1:\","
                     "        \"question\": \"The header!\","
                     "        \"inproduct_target\": \"The button label!\","
                     "        \"inproduct\": \"http://link.com\","
                     "        \"tooltip\": \"No thanks, hide this.\""
                     "       }"
                     "    ]"
                     "  }"
                     "}";
  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Initialize a message loop for this to run on.
  MessageLoop loop;

  // Check that prefs are set correctly.
  web_resource_service_->UnpackWebStoreSignal(*(test_json.get()));
  PrefService* prefs = profile_.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  EXPECT_EQ("341252", AppsPromo::GetPromoId());
  EXPECT_EQ("The header!", AppsPromo::GetPromoHeaderText());
  EXPECT_EQ("The button label!", AppsPromo::GetPromoButtonText());
  EXPECT_EQ(GURL("http://link.com"), AppsPromo::GetPromoLink());
  EXPECT_EQ("No thanks, hide this.", AppsPromo::GetPromoExpireText());
  EXPECT_EQ(AppsPromo::USERS_NEW, AppsPromo::GetPromoUserGroup());
  EXPECT_EQ(GURL("chrome://theme/IDR_WEBSTORE_ICON"),
            AppsPromo::GetPromoLogo());
}

// Tests that the "web store active" flag is set even when the web store promo
// fails parsing.
TEST_F(PromoResourceServiceTest, UnpackPartialWebStoreSignal) {
  std::string json = "{ "
                     "  \"topic\": {"
                     "    \"answers\": ["
                     "       {"
                     "        \"answer_id\": \"sdlfj32\","
                     "        \"name\": \"webstore_promo:#klsdjlfSD\""
                     "       }"
                     "    ]"
                     "  }"
                     "}";
  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Initialize a message loop for this to run on.
  MessageLoop loop;

  // Check that prefs are set correctly.
  web_resource_service_->UnpackWebStoreSignal(*(test_json.get()));
  EXPECT_FALSE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_TRUE(AppsPromo::IsWebStoreSupportedForLocale());
}

TEST_F(PromoResourceServiceTest, IsBuildTargeted) {
  // canary
  const chrome::VersionInfo::Channel canary =
      chrome::VersionInfo::CHANNEL_CANARY;
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(canary, 1));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(canary, 3));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(canary, 7));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(canary, 15));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(canary, 8));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(canary, 11));

  // dev
  const chrome::VersionInfo::Channel dev =
      chrome::VersionInfo::CHANNEL_DEV;
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 1));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 3));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 7));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 15));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(dev, 8));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 11));

  // beta
  const chrome::VersionInfo::Channel beta =
      chrome::VersionInfo::CHANNEL_BETA;
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(beta, 1));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(beta, 3));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(beta, 7));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(beta, 15));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(beta, 8));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(beta, 11));

  // stable
  const chrome::VersionInfo::Channel stable =
      chrome::VersionInfo::CHANNEL_STABLE;
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, 1));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, 3));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(stable, 7));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(stable, 15));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, 8));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, 11));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(stable, 12));
}
