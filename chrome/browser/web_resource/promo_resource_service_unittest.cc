// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/browser/web_resource/promo_resource_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PromoResourceServiceTest : public testing::Test {
 public:
  TestingPrefService* local_state() { return &local_state_; }
  PromoResourceServiceFactory* promo_resource_service_factory() {
      return promo_resource_service_factory_;
  }
  PromoResourceService* web_resource_service() {
      return web_resource_service_;
  }

  int promo_group() {
    return web_resource_service_->promo_group_;
  }

 protected:
  PromoResourceServiceTest() {}
  virtual ~PromoResourceServiceTest() {}

  // testing::Test
  virtual void SetUp();
  virtual void TearDown();

 private:
  // weak Singleton
  PromoResourceServiceFactory* promo_resource_service_factory_;
  scoped_refptr<PromoResourceService> web_resource_service_;
  TestingPrefService local_state_;
};

void PromoResourceServiceTest::SetUp() {
  // Set up the local state preferences.
  browser::RegisterLocalState(&local_state_);
  TestingBrowserProcess* testing_browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  testing_browser_process->SetPrefService(&local_state_);

  promo_resource_service_factory_ =
      PromoResourceServiceFactory::GetInstance();
  web_resource_service_ =
    promo_resource_service_factory_->promo_resource_service();
  // To get around the weirdness of using a Singleton across multiple tests, we
  // may need to initialize the WebResourceService local state here.
  if (!local_state()->FindPreference(prefs::kNTPPromoBuild))
    web_resource_service_->Init();
}

void PromoResourceServiceTest::TearDown() {
  TestingBrowserProcess* testing_browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  testing_browser_process->SetPrefService(NULL);
}

namespace {

// From promo_resource_service.cc
enum BuildType {
  DEV_BUILD = 1,
  BETA_BUILD = 1 << 1,
  STABLE_BUILD = 1 << 2,
};

}  // namespace

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
  web_resource_service()->UnpackLogoSignal(*(test_json.get()));

  double logo_start =
      local_state()->GetDouble(prefs::kNTPCustomLogoStart);
  EXPECT_EQ(logo_start, 1264899600);  // unix epoch for Jan 31 2010 0100 GMT.
  double logo_end =
      local_state()->GetDouble(prefs::kNTPCustomLogoEnd);
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
  web_resource_service()->UnpackLogoSignal(*(test_json.get()));

  logo_start = local_state()->GetDouble(prefs::kNTPCustomLogoStart);
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
  web_resource_service()->UnpackLogoSignal(*(test_json.get()));
  logo_start = local_state()->GetDouble(prefs::kNTPCustomLogoStart);
  EXPECT_EQ(logo_start, 0);  // date value reset to 0;
  logo_end = local_state()->GetDouble(prefs::kNTPCustomLogoEnd);
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
  web_resource_service()->UnpackPromoSignal(*(test_json.get()));

  std::string promo_line = local_state()->GetString(prefs::kNTPPromoLine);
  EXPECT_EQ(promo_line, "Eat more pie!");

  int promo_build_type = local_state()->GetInteger(prefs::kNTPPromoBuild);
  EXPECT_EQ(promo_build_type & DEV_BUILD, DEV_BUILD);
  EXPECT_EQ(promo_build_type & BETA_BUILD, BETA_BUILD);
  EXPECT_EQ(promo_build_type & STABLE_BUILD, 0);

  double promo_start =
      local_state()->GetDouble(prefs::kNTPPromoStart);  // In seconds.
  int timeslice = 2;  // From the second part of the "question" field.
  // Start date for group 0, unix epoch for Jan 31 2010 0100 GMT.
  int64 start_date = 1264899600;
  // Add promo_group * timeslice converted to seconds for actual start.
  EXPECT_EQ(promo_start, start_date + promo_group() * timeslice * 60 * 60);

  double promo_end =
      local_state()->GetDouble(prefs::kNTPPromoEnd);
  EXPECT_EQ(promo_end, 1327971600);  // unix epoch for Jan 31 2012 0100 GMT.
}


