// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_unittest_base.h"
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

InstantUnitTestBase::InstantUnitTestBase() {
  field_trial_list_.reset(new base::FieldTrialList(
      new metrics::SHA1EntropyProvider("42")));
}

InstantUnitTestBase::~InstantUnitTestBase() {
}

void InstantUnitTestBase::SetUp() {
  SetUpHelper();
}

void InstantUnitTestBase::SetUpWithoutCacheableNTP() {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 use_cacheable_ntp:0"));
  SetUpHelper();
}

void InstantUnitTestBase::SetUpHelper() {
  chrome::EnableInstantExtendedAPIForTesting();
  BrowserWithTestWindowTest::SetUp();

  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &TemplateURLServiceFactory::BuildInstanceFor);
  template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service_);

  UIThreadSearchTermsData::SetGoogleBaseURL("https://www.google.com/");
  TestingPrefServiceSyncable* pref_service = profile()->GetTestingPrefService();
  pref_service->SetUserPref(prefs::kLastPromptedGoogleURL,
                            new base::StringValue("https://www.google.com/"));
  SetDefaultSearchProvider("{google:baseURL}");
  instant_service_ = InstantServiceFactory::GetForProfile(profile());
}

void InstantUnitTestBase::TearDown() {
  UIThreadSearchTermsData::SetGoogleBaseURL("");
  BrowserWithTestWindowTest::TearDown();
}

void InstantUnitTestBase::SetDefaultSearchProvider(
    const std::string& base_url) {
  TemplateURLData data;
  data.SetURL(base_url + "url?bar={searchTerms}");
  data.instant_url = base_url + "instant?"
                     "{google:omniboxStartMarginParameter}foo=foo#foo=foo&strk";
  data.new_tab_url = base_url + "newtab";
  data.alternate_urls.push_back(base_url + "alt#quux={searchTerms}");
  data.search_terms_replacement_key = "strk";

  TemplateURL* template_url = new TemplateURL(profile(), data);
  // Takes ownership of |template_url|.
  template_url_service_->Add(template_url);
  template_url_service_->SetDefaultSearchProvider(template_url);
}

void InstantUnitTestBase::NotifyGoogleBaseURLUpdate(
    const std::string& new_google_base_url) {
  // GoogleURLTracker is not created in tests.
  // (See GoogleURLTrackerFactory::ServiceIsNULLWhileTesting())
  // For determining google:baseURL for NTP, the following is used:
  // UIThreadSearchTermsData::GoogleBaseURLValue()
  // For simulating test behavior, this is overridden below.
  UIThreadSearchTermsData::SetGoogleBaseURL(new_google_base_url);
  GoogleURLTracker::UpdatedDetails details(GURL("https://www.google.com/"),
                                           GURL(new_google_base_url));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
      content::Source<Profile>(profile()->GetOriginalProfile()),
      content::Details<GoogleURLTracker::UpdatedDetails>(&details));
}


bool InstantUnitTestBase::IsInstantServiceObserver(
    InstantServiceObserver* observer) {
  return instant_service_->observers_.HasObserver(observer);
}

