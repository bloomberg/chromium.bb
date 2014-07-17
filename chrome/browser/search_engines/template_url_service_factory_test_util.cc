// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"

#include "base/run_loop.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/default_search_pref_test_util.h"
#include "components/search_engines/template_url_service.h"

TemplateURLServiceFactoryTestUtil::TemplateURLServiceFactoryTestUtil(
    TestingProfile* profile)
    : profile_(profile) {
  profile_->CreateWebDataService();

  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_, TemplateURLServiceFactory::BuildInstanceFor);
}

TemplateURLServiceFactoryTestUtil::~TemplateURLServiceFactoryTestUtil() {
  // Flush the message loop to make application verifiers happy.
  base::RunLoop().RunUntilIdle();
}

void TemplateURLServiceFactoryTestUtil::VerifyLoad() {
  model()->Load();
  base::RunLoop().RunUntilIdle();
}

void TemplateURLServiceFactoryTestUtil::SetManagedDefaultSearchPreferences(
    bool enabled,
    const std::string& name,
    const std::string& keyword,
    const std::string& search_url,
    const std::string& suggest_url,
    const std::string& icon_url,
    const std::string& encodings,
    const std::string& alternate_url,
    const std::string& search_terms_replacement_key) {
  DefaultSearchPrefTestUtil::SetManagedPref(
      profile_->GetTestingPrefService(),
      enabled, name, keyword, search_url, suggest_url, icon_url, encodings,
      alternate_url, search_terms_replacement_key);
}

void
TemplateURLServiceFactoryTestUtil::RemoveManagedDefaultSearchPreferences() {
  DefaultSearchPrefTestUtil::RemoveManagedPref(
      profile_->GetTestingPrefService());
}

TemplateURLService* TemplateURLServiceFactoryTestUtil::model() const {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}
