// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_test_util.h"

#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/default_search_pref_test_util.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/testing_search_terms_data.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestingTemplateURLServiceClient : public ChromeTemplateURLServiceClient {
 public:
  TestingTemplateURLServiceClient(Profile* profile,
                                  base::string16* search_term)
      : ChromeTemplateURLServiceClient(profile),
        search_term_(search_term) {}

  virtual void SetKeywordSearchTermsForURL(
      const GURL& url,
      TemplateURLID id,
      const base::string16& term) OVERRIDE {
    *search_term_ = term;
  }

 private:
  base::string16* search_term_;

  DISALLOW_COPY_AND_ASSIGN(TestingTemplateURLServiceClient);
};

}  // namespace

TemplateURLServiceTestUtil::TemplateURLServiceTestUtil()
    : changed_count_(0),
      search_terms_data_(NULL) {
  // Make unique temp directory.
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  profile_.reset(new TestingProfile(temp_dir_.path()));

  scoped_refptr<WebDatabaseService> web_database_service =
      new WebDatabaseService(temp_dir_.path().AppendASCII("webdata"),
                             base::MessageLoopProxy::current(),
                             base::MessageLoopProxy::current());
  web_database_service->AddTable(
      scoped_ptr<WebDatabaseTable>(new KeywordTable()));
  web_database_service->LoadDatabase();

  web_data_service_ =  new KeywordWebDataService(
      web_database_service.get(), base::MessageLoopProxy::current(),
      KeywordWebDataService::ProfileErrorCallback());
  web_data_service_->Init();

  ResetModel(false);
}

TemplateURLServiceTestUtil::~TemplateURLServiceTestUtil() {
  ClearModel();
  profile_.reset();

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

  model()->web_data_service_ = web_data_service_;
  base::RunLoop().RunUntilIdle();
}

void TemplateURLServiceTestUtil::ClearModel() {
  model_->Shutdown();
  model_.reset();
  search_terms_data_ = NULL;
}

void TemplateURLServiceTestUtil::ResetModel(bool verify_load) {
  if (model_)
    ClearModel();
  search_terms_data_ = new TestingSearchTermsData("http://www.google.com/");
  model_.reset(new TemplateURLService(
      profile()->GetPrefs(), scoped_ptr<SearchTermsData>(search_terms_data_),
      web_data_service_.get(),
      scoped_ptr<TemplateURLServiceClient>(
          new TestingTemplateURLServiceClient(profile(), &search_term_)),
      NULL, NULL, base::Closure()));
  model()->AddObserver(this);
  changed_count_ = 0;
  if (verify_load)
    VerifyLoad();
}

base::string16 TemplateURLServiceTestUtil::GetAndClearSearchTerm() {
  base::string16 search_term;
  search_term.swap(search_term_);
  return search_term;
}

void TemplateURLServiceTestUtil::SetGoogleBaseURL(const GURL& base_url) {
  DCHECK(base_url.is_valid());
  search_terms_data_->set_google_base_url(base_url.spec());
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
