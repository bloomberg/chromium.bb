// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/web_resource/web_resource_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test WebResourceServiceTest;

namespace {

// From web_resource_service.cc
enum BuildType {
  DEV_BUILD = 1,
  BETA_BUILD = 1 << 1,
  STABLE_BUILD = 1 << 2,
};

}  // namespace

// Verifies that custom dates read from a web resource server are written to
// the preferences file.
TEST_F(WebResourceServiceTest, UnpackLogoSignal) {
  // Set up a testing profile and create a web resource service.
  TestingProfile profile;
  scoped_refptr<WebResourceService> web_resource_service(
      new WebResourceService(&profile));

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
  web_resource_service->UnpackLogoSignal(*(test_json.get()));
  PrefService* prefs = profile.GetPrefs();
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
  web_resource_service->UnpackLogoSignal(*(test_json.get()));

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
  web_resource_service->UnpackLogoSignal(*(test_json.get()));
  logo_start = prefs->GetDouble(prefs::kNTPCustomLogoStart);
  EXPECT_EQ(logo_start, 0);  // date value reset to 0;
  logo_end = prefs->GetDouble(prefs::kNTPCustomLogoEnd);
  EXPECT_EQ(logo_end, 0);  // date value reset to 0;
}

TEST_F(WebResourceServiceTest, UnpackPromoSignal) {
  // Set up a testing profile and create a web resource service.
  TestingProfile profile;
  scoped_refptr<WebResourceService> web_resource_service(
      new WebResourceService(&profile));

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
  web_resource_service->UnpackPromoSignal(*(test_json.get()));
  PrefService* prefs = profile.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  std::string promo_line = prefs->GetString(prefs::kNTPPromoLine);
  EXPECT_EQ(promo_line, "Eat more pie!");

  int promo_group = prefs->GetInteger(prefs::kNTPPromoGroup);
  EXPECT_GE(promo_group, 0);
  EXPECT_LT(promo_group, 16);

  int promo_build_type = prefs->GetInteger(prefs::kNTPPromoBuild);
  EXPECT_EQ(promo_build_type & DEV_BUILD, DEV_BUILD);
  EXPECT_EQ(promo_build_type & BETA_BUILD, BETA_BUILD);
  EXPECT_EQ(promo_build_type & STABLE_BUILD, 0);

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


