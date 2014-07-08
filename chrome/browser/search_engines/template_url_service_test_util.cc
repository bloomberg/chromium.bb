// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_test_util.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/threading/thread.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/google/google_brand_chromeos.h"
#endif

// Trivial subclass of TemplateURLService that records the last invocation of
// SetKeywordSearchTermsForURL.
class TestingTemplateURLService : public TemplateURLService {
 public:
  static KeyedService* Build(content::BrowserContext* profile) {
    return new TestingTemplateURLService(static_cast<Profile*>(profile));
  }

  explicit TestingTemplateURLService(Profile* profile)
      : TemplateURLService(
            profile->GetPrefs(),
            scoped_ptr<SearchTermsData>(new UIThreadSearchTermsData(profile)),
            WebDataServiceFactory::GetKeywordWebDataForProfile(
                profile, Profile::EXPLICIT_ACCESS),
            scoped_ptr<TemplateURLServiceClient>(
                new ChromeTemplateURLServiceClient(profile)), NULL, NULL,
            base::Closure()) {
  }

  base::string16 GetAndClearSearchTerm() {
    base::string16 search_term;
    search_term.swap(search_term_);
    return search_term;
  }

 protected:
  virtual void SetKeywordSearchTermsForURL(
      const TemplateURL* t_url,
      const GURL& url,
      const base::string16& term) OVERRIDE {
    search_term_ = term;
  }

 private:
  base::string16 search_term_;

  DISALLOW_COPY_AND_ASSIGN(TestingTemplateURLService);
};

// TemplateURLServiceTestUtilBase ---------------------------------------------

TemplateURLServiceTestUtilBase::TemplateURLServiceTestUtilBase()
    : changed_count_(0) {
}

TemplateURLServiceTestUtilBase::~TemplateURLServiceTestUtilBase() {
}

void TemplateURLServiceTestUtilBase::CreateTemplateUrlService() {
  profile()->CreateWebDataService();

  TemplateURLService* service = static_cast<TemplateURLService*>(
      TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), TestingTemplateURLService::Build));
  service->AddObserver(this);
}

void TemplateURLServiceTestUtilBase::OnTemplateURLServiceChanged() {
  changed_count_++;
}

int TemplateURLServiceTestUtilBase::GetObserverCount() {
  return changed_count_;
}

void TemplateURLServiceTestUtilBase::ResetObserverCount() {
  changed_count_ = 0;
}

void TemplateURLServiceTestUtilBase::VerifyLoad() {
  ASSERT_FALSE(model()->loaded());
  model()->Load();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetObserverCount());
  ResetObserverCount();
}

void TemplateURLServiceTestUtilBase::ChangeModelToLoadState() {
  model()->ChangeToLoadedState();
  // Initialize the web data service so that the database gets updated with
  // any changes made.

  model()->web_data_service_ =
      WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile(), Profile::EXPLICIT_ACCESS);
  base::RunLoop().RunUntilIdle();
}

void TemplateURLServiceTestUtilBase::ClearModel() {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile(), NULL);
}

void TemplateURLServiceTestUtilBase::ResetModel(bool verify_load) {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), TestingTemplateURLService::Build);
  model()->AddObserver(this);
  changed_count_ = 0;
  if (verify_load)
    VerifyLoad();
}

base::string16 TemplateURLServiceTestUtilBase::GetAndClearSearchTerm() {
  return
      static_cast<TestingTemplateURLService*>(model())->GetAndClearSearchTerm();
}

void TemplateURLServiceTestUtilBase::SetGoogleBaseURL(
    const GURL& base_url) const {
  DCHECK(base_url.is_valid());
  UIThreadSearchTermsData data(profile());
  UIThreadSearchTermsData::SetGoogleBaseURL(base_url.spec());
  TemplateURLServiceFactory::GetForProfile(profile())->GoogleBaseURLChanged();
}

void TemplateURLServiceTestUtilBase::SetManagedDefaultSearchPreferences(
    bool enabled,
    const std::string& name,
    const std::string& keyword,
    const std::string& search_url,
    const std::string& suggest_url,
    const std::string& icon_url,
    const std::string& encodings,
    const std::string& alternate_url,
    const std::string& search_terms_replacement_key) {
  TestingPrefServiceSyncable* pref_service = profile()->GetTestingPrefService();
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  if (!enabled) {
    value->SetBoolean(DefaultSearchManager::kDisabledByPolicy, true);
    pref_service->SetManagedPref(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName,
        value.release());
    return;
  }

  EXPECT_FALSE(keyword.empty());
  EXPECT_FALSE(search_url.empty());
  value->Set(DefaultSearchManager::kShortName,
             base::Value::CreateStringValue(name));
  value->Set(DefaultSearchManager::kKeyword,
             base::Value::CreateStringValue(keyword));
  value->Set(DefaultSearchManager::kURL,
             base::Value::CreateStringValue(search_url));
  value->Set(DefaultSearchManager::kSuggestionsURL,
             base::Value::CreateStringValue(suggest_url));
  value->Set(DefaultSearchManager::kFaviconURL,
             base::Value::CreateStringValue(icon_url));
  value->Set(DefaultSearchManager::kSearchTermsReplacementKey,
             base::Value::CreateStringValue(search_terms_replacement_key));

  std::vector<std::string> encodings_items;
  base::SplitString(encodings, ';', &encodings_items);
  scoped_ptr<base::ListValue> encodings_list(new base::ListValue);
  for (std::vector<std::string>::const_iterator it = encodings_items.begin();
       it != encodings_items.end();
       ++it) {
    encodings_list->AppendString(*it);
  }
  value->Set(DefaultSearchManager::kInputEncodings, encodings_list.release());

  scoped_ptr<base::ListValue> alternate_url_list(new base::ListValue());
  if (!alternate_url.empty())
    alternate_url_list->Append(base::Value::CreateStringValue(alternate_url));
  value->Set(DefaultSearchManager::kAlternateURLs,
             alternate_url_list.release());

  pref_service->SetManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      value.release());
}

void TemplateURLServiceTestUtilBase::RemoveManagedDefaultSearchPreferences() {
  TestingPrefServiceSyncable* pref_service = profile()->GetTestingPrefService();
  pref_service->RemoveManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
}

TemplateURLService* TemplateURLServiceTestUtilBase::model() const {
  return TemplateURLServiceFactory::GetForProfile(profile());
}


// TemplateURLServiceTestUtil -------------------------------------------------

TemplateURLServiceTestUtil::TemplateURLServiceTestUtil()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
}

TemplateURLServiceTestUtil::~TemplateURLServiceTestUtil() {
}

void TemplateURLServiceTestUtil::SetUp() {
  // Make unique temp directory.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  profile_.reset(new TestingProfile(temp_dir_.path()));

  TemplateURLServiceTestUtilBase::CreateTemplateUrlService();

#if defined(OS_CHROMEOS)
  google_brand::chromeos::ClearBrandForCurrentSession();
#endif
}

void TemplateURLServiceTestUtil::TearDown() {
  profile_.reset();

  UIThreadSearchTermsData::SetGoogleBaseURL(std::string());

  // Flush the message loop to make application verifiers happy.
  base::RunLoop().RunUntilIdle();
}

TestingProfile* TemplateURLServiceTestUtil::profile() const {
  return profile_.get();
}
