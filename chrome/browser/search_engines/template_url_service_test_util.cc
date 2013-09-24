// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_test_util.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/google/google_util_chromeos.h"
#endif

// Trivial subclass of TemplateURLService that records the last invocation of
// SetKeywordSearchTermsForURL.
class TestingTemplateURLService : public TemplateURLService {
 public:
  static BrowserContextKeyedService* Build(content::BrowserContext* profile) {
    return new TestingTemplateURLService(static_cast<Profile*>(profile));
  }

  explicit TestingTemplateURLService(Profile* profile)
      : TemplateURLService(profile) {
  }

  string16 GetAndClearSearchTerm() {
    string16 search_term;
    search_term.swap(search_term_);
    return search_term;
  }

 protected:
  virtual void SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                           const GURL& url,
                                           const string16& term) OVERRIDE {
    search_term_ = term;
  }

 private:
  string16 search_term_;

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

  model()->service_ = WebDataService::FromBrowserContext(profile());
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

string16 TemplateURLServiceTestUtilBase::GetAndClearSearchTerm() {
  return
      static_cast<TestingTemplateURLService*>(model())->GetAndClearSearchTerm();
}

void TemplateURLServiceTestUtilBase::SetGoogleBaseURL(
    const GURL& base_url) const {
  DCHECK(base_url.is_valid());
  UIThreadSearchTermsData data(profile());
  GoogleURLTracker::UpdatedDetails urls(GURL(data.GoogleBaseURLValue()),
                                        base_url);
  UIThreadSearchTermsData::SetGoogleBaseURL(base_url.spec());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
      content::Source<Profile>(profile()),
      content::Details<GoogleURLTracker::UpdatedDetails>(&urls));
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
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderEnabled,
                               Value::CreateBooleanValue(enabled));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderName,
                               Value::CreateStringValue(name));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderKeyword,
                               Value::CreateStringValue(keyword));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderSearchURL,
                               Value::CreateStringValue(search_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderSuggestURL,
                               Value::CreateStringValue(suggest_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderIconURL,
                               Value::CreateStringValue(icon_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderEncodings,
                               Value::CreateStringValue(encodings));
  scoped_ptr<base::ListValue> alternate_url_list(new base::ListValue());
  if (!alternate_url.empty())
    alternate_url_list->Append(Value::CreateStringValue(alternate_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderAlternateURLs,
                               alternate_url_list.release());
  pref_service->SetManagedPref(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      Value::CreateStringValue(search_terms_replacement_key));
  model()->Observe(chrome::NOTIFICATION_DEFAULT_SEARCH_POLICY_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
}

void TemplateURLServiceTestUtilBase::RemoveManagedDefaultSearchPreferences() {
  TestingPrefServiceSyncable* pref_service = profile()->GetTestingPrefService();
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderEnabled);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderName);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderKeyword);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderSearchURL);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderSuggestURL);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderIconURL);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderEncodings);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderAlternateURLs);
  pref_service->RemoveManagedPref(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderID);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderPrepopulateID);
  model()->Observe(chrome::NOTIFICATION_DEFAULT_SEARCH_POLICY_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
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
  google_util::chromeos::ClearBrandForCurrentSession();
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
