// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_test_util.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/threading/thread.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/default_search_pref_test_util.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/google/google_brand_chromeos.h"
#endif

// Trivial subclass of TemplateURLService that records the last invocation of
// SetKeywordSearchTermsForURL.
class TestingTemplateURLService : public TemplateURLService {
 public:
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


TemplateURLServiceTestUtil::TemplateURLServiceTestUtil()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      changed_count_(0) {
  // Make unique temp directory.
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  profile_.reset(new TestingProfile(temp_dir_.path()));

  profile()->CreateWebDataService();

  model_.reset(new TestingTemplateURLService(profile()));
  model_->AddObserver(this);

#if defined(OS_CHROMEOS)
  google_brand::chromeos::ClearBrandForCurrentSession();
#endif
}

TemplateURLServiceTestUtil::~TemplateURLServiceTestUtil() {
  ClearModel();
  profile_.reset();

  UIThreadSearchTermsData::SetGoogleBaseURL(std::string());

  // Flush the message loop to make application verifiers happy.
  base::RunLoop().RunUntilIdle();
}

void TemplateURLServiceTestUtil::OnTemplateURLServiceChanged() {
  changed_count_++;
}

int TemplateURLServiceTestUtil::GetObserverCount() {
  return changed_count_;
}

void TemplateURLServiceTestUtil::ResetObserverCount() {
  changed_count_ = 0;
}

void TemplateURLServiceTestUtil::VerifyLoad() {
  ASSERT_FALSE(model()->loaded());
  model()->Load();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetObserverCount());
  ResetObserverCount();
}

void TemplateURLServiceTestUtil::ChangeModelToLoadState() {
  model()->ChangeToLoadedState();
  // Initialize the web data service so that the database gets updated with
  // any changes made.

  model()->web_data_service_ =
      WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile(), Profile::EXPLICIT_ACCESS);
  base::RunLoop().RunUntilIdle();
}

void TemplateURLServiceTestUtil::ClearModel() {
  model_->Shutdown();
  model_.reset();
}

void TemplateURLServiceTestUtil::ResetModel(bool verify_load) {
  if (model_)
    ClearModel();
  model_.reset(new TestingTemplateURLService(profile()));
  model()->AddObserver(this);
  changed_count_ = 0;
  if (verify_load)
    VerifyLoad();
}

base::string16 TemplateURLServiceTestUtil::GetAndClearSearchTerm() {
  return model_->GetAndClearSearchTerm();
}

void TemplateURLServiceTestUtil::SetGoogleBaseURL(const GURL& base_url) {
  DCHECK(base_url.is_valid());
  UIThreadSearchTermsData data(profile());
  UIThreadSearchTermsData::SetGoogleBaseURL(base_url.spec());
  model_->GoogleBaseURLChanged();
}

void TemplateURLServiceTestUtil::SetManagedDefaultSearchPreferences(
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
      profile()->GetTestingPrefService(),
      enabled, name, keyword, search_url, suggest_url, icon_url, encodings,
      alternate_url, search_terms_replacement_key);
}

void TemplateURLServiceTestUtil::RemoveManagedDefaultSearchPreferences() {
  DefaultSearchPrefTestUtil::RemoveManagedPref(
      profile()->GetTestingPrefService());
}

TemplateURLService* TemplateURLServiceTestUtil::model() {
  return model_.get();
}
