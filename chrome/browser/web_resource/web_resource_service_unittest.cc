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
                     "        \"custom_logo_start\": \"31/01/10 01:00 GMT\","
                     "        \"custom_logo_end\": \"31/01/12 01:00 GMT\""
                     "       }"
                     "    ]"
                     "  }"
                     "}";
  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Check that prefs are set correctly.
  web_resource_service->UnpackLogoSignal(*(test_json.get()));
  double logo_start =
      profile.GetPrefs()->GetReal(prefs::kNTPCustomLogoStart);
  ASSERT_EQ(logo_start, 1264899600);  // unix epoch for Jan 31 2010 0100 GMT.
  double logo_end =
      profile.GetPrefs()->GetReal(prefs::kNTPCustomLogoEnd);
  ASSERT_EQ(logo_end, 1327971600);  // unix epoch for Jan 31 2012 0100 GMT.

  // Change the start only and recheck.
  json = "{ "
         "  \"topic\": {"
         "    \"answers\": ["
         "       {"
         "        \"custom_logo_start\": \"28/02/10 14:00 GMT\","
         "        \"custom_logo_end\": \"31/01/12 01:00 GMT\""
         "       }"
         "    ]"
         "  }"
         "}";
  test_json->Clear();
  test_json.reset(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Check that prefs are set correctly.
  web_resource_service->UnpackLogoSignal(*(test_json.get()));
  logo_start = profile.GetPrefs()->GetReal(prefs::kNTPCustomLogoStart);
  ASSERT_EQ(logo_start, 1267365600);  // date changes to Feb 28 2010 1400 GMT.

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
  logo_start = profile.GetPrefs()->GetReal(prefs::kNTPCustomLogoStart);
  ASSERT_EQ(logo_start, 0);  // date value reset to 0;
  logo_end = profile.GetPrefs()->GetReal(prefs::kNTPCustomLogoEnd);
  ASSERT_EQ(logo_end, 0);  // date value reset to 0;
}

