// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/search_engines/search_provider_install_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

// TestGetInstallState --------------------------------------------------------

// Test the SearchProviderInstallData::GetInstallState.
class TestGetInstallState {
 public:
  explicit TestGetInstallState(SearchProviderInstallData* install_data);

  // Runs all of the test cases.
  void RunTests(const std::string& search_provider_host,
                const std::string& default_search_provider_host);

 private:
  // Callback for when SearchProviderInstallData is ready to have
  // GetInstallState called. Runs all of the test cases.
  void DoInstallStateTests(const std::string& search_provider_host,
                           const std::string& default_search_provider_host);

  // Does a verification for one url and its expected state.
  void VerifyInstallState(SearchProviderInstallData::State expected_state,
                          const std::string& url);

  SearchProviderInstallData* install_data_;

  DISALLOW_COPY_AND_ASSIGN(TestGetInstallState);
};

TestGetInstallState::TestGetInstallState(
    SearchProviderInstallData* install_data)
    : install_data_(install_data) {
}

void TestGetInstallState::RunTests(
    const std::string& search_provider_host,
    const std::string& default_search_provider_host) {
  install_data_->CallWhenLoaded(
      base::Bind(&TestGetInstallState::DoInstallStateTests,
                 base::Unretained(this),
                 search_provider_host, default_search_provider_host));
  base::RunLoop().RunUntilIdle();
}

void TestGetInstallState::DoInstallStateTests(
    const std::string& search_provider_host,
    const std::string& default_search_provider_host) {
  SCOPED_TRACE("search provider: " + search_provider_host +
               ", default search provider: " + default_search_provider_host);
  // Installed but not default.
  VerifyInstallState(SearchProviderInstallData::INSTALLED_BUT_NOT_DEFAULT,
                     "http://" + search_provider_host + "/");
  VerifyInstallState(SearchProviderInstallData::INSTALLED_BUT_NOT_DEFAULT,
                     "http://" + search_provider_host + ":80/");

  // Not installed.
  VerifyInstallState(SearchProviderInstallData::NOT_INSTALLED,
                     "http://" + search_provider_host + ":96/");

  // Not installed due to different scheme.
  VerifyInstallState(SearchProviderInstallData::NOT_INSTALLED,
                     "https://" + search_provider_host + "/");

  // Not installed.
  VerifyInstallState(SearchProviderInstallData::NOT_INSTALLED,
                     "http://a" + search_provider_host + "/");

  // Installed as default.
  if (!default_search_provider_host.empty()) {
    VerifyInstallState(SearchProviderInstallData::INSTALLED_AS_DEFAULT,
                       "http://" + default_search_provider_host + "/");
  }
}

void TestGetInstallState::VerifyInstallState(
    SearchProviderInstallData::State expected_state,
    const std::string& url) {

  SearchProviderInstallData::State actual_state =
      install_data_->GetInstallState(GURL(url));
  EXPECT_EQ(expected_state, actual_state)
      << "GetInstallState for " << url << " failed. Expected "
      << expected_state << ".  Actual " << actual_state << ".";
}

}  // namespace

// SearchProviderInstallDataTest ----------------------------------------------

// Provides basic test set-up/tear-down functionality needed by all tests
// that use TemplateURLServiceTestUtil.
class SearchProviderInstallDataTest : public testing::Test {
 public:
  SearchProviderInstallDataTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void SimulateDefaultSearchIsManaged(const std::string& url);

 protected:
  TemplateURL* AddNewTemplateURL(const std::string& url,
                                 const base::string16& keyword);

  TemplateURLServiceTestUtil util_;

  // Provides the search provider install state on the I/O thread. It must be
  // deleted on the I/O thread, which is why it isn't a scoped_ptr.
  SearchProviderInstallData* install_data_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderInstallDataTest);
};

SearchProviderInstallDataTest::SearchProviderInstallDataTest()
    : install_data_(NULL) {
}

void SearchProviderInstallDataTest::SetUp() {
  testing::Test::SetUp();
#if defined(OS_ANDROID)
  TemplateURLPrepopulateData::InitCountryCode(
      std::string() /* unknown country code */);
#endif
  util_.SetUp();
  install_data_ = new SearchProviderInstallData(util_.profile(),
      content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
      content::Source<SearchProviderInstallDataTest>(this));
}

void SearchProviderInstallDataTest::TearDown() {
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, install_data_);
  install_data_ = NULL;

  // Make sure that the install data class on the UI thread gets cleaned up.
  // It doesn't matter that this happens after install_data_ is deleted.
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
      content::Source<SearchProviderInstallDataTest>(this),
      content::NotificationService::NoDetails());

  util_.TearDown();
  testing::Test::TearDown();
}

void SearchProviderInstallDataTest::SimulateDefaultSearchIsManaged(
    const std::string& url) {
  ASSERT_FALSE(url.empty());
  TestingPrefServiceSyncable* service =
      util_.profile()->GetTestingPrefService();
  service->SetManagedPref(prefs::kDefaultSearchProviderEnabled,
                          Value::CreateBooleanValue(true));
  service->SetManagedPref(prefs::kDefaultSearchProviderSearchURL,
                          Value::CreateStringValue(url));
  service->SetManagedPref(prefs::kDefaultSearchProviderName,
                          Value::CreateStringValue("managed"));
  // Clear the IDs that are not specified via policy.
  service->SetManagedPref(prefs::kDefaultSearchProviderID,
                          new StringValue(std::string()));
  service->SetManagedPref(prefs::kDefaultSearchProviderPrepopulateID,
                          new StringValue(std::string()));
  util_.model()->Observe(chrome::NOTIFICATION_DEFAULT_SEARCH_POLICY_CHANGED,
                         content::NotificationService::AllSources(),
                         content::NotificationService::NoDetails());
}

TemplateURL* SearchProviderInstallDataTest::AddNewTemplateURL(
    const std::string& url,
    const base::string16& keyword) {
  TemplateURLData data;
  data.short_name = keyword;
  data.SetKeyword(keyword);
  data.SetURL(url);
  TemplateURL* t_url = new TemplateURL(util_.profile(), data);
  util_.model()->Add(t_url);
  return t_url;
}

// Actual tests ---------------------------------------------------------------

TEST_F(SearchProviderInstallDataTest, GetInstallState) {
  // Set up the database.
  util_.ChangeModelToLoadState();
  std::string host = "www.unittest.com";
  AddNewTemplateURL("http://" + host + "/path", ASCIIToUTF16("unittest"));

  // Wait for the changes to be saved.
  base::RunLoop().RunUntilIdle();

  // Verify the search providers install state (with no default set).
  TestGetInstallState test_get_install_state(install_data_);
  test_get_install_state.RunTests(host, std::string());

  // Set-up a default and try it all one more time.
  std::string default_host = "www.mmm.com";
  TemplateURL* default_url =
      AddNewTemplateURL("http://" + default_host + "/", ASCIIToUTF16("mmm"));
  util_.model()->SetDefaultSearchProvider(default_url);
  test_get_install_state.RunTests(host, default_host);
}

TEST_F(SearchProviderInstallDataTest, ManagedDefaultSearch) {
  // Set up the database.
  util_.ChangeModelToLoadState();
  std::string host = "www.unittest.com";
  AddNewTemplateURL("http://" + host + "/path", ASCIIToUTF16("unittest"));

  // Set a managed preference that establishes a default search provider.
  std::string host2 = "www.managedtest.com";
  std::string url2 = "http://" + host2 + "/p{searchTerms}";
  SimulateDefaultSearchIsManaged(url2);
  EXPECT_TRUE(util_.model()->is_default_search_managed());

  // Wait for the changes to be saved.
  base::RunLoop().RunUntilIdle();

  // Verify the search providers install state.  The default search should be
  // the managed one we previously set.
  TestGetInstallState test_get_install_state(install_data_);
  test_get_install_state.RunTests(host, host2);
}

TEST_F(SearchProviderInstallDataTest, GoogleBaseUrlChange) {
  TestGetInstallState test_get_install_state(install_data_);

  // Set up the database.
  util_.ChangeModelToLoadState();
  std::string google_host = "w.com";
  util_.SetGoogleBaseURL(GURL("http://" + google_host + "/"));
  // Wait for the I/O thread to process the update notification.
  base::RunLoop().RunUntilIdle();

  AddNewTemplateURL("{google:baseURL}?q={searchTerms}", ASCIIToUTF16("t"));
  TemplateURL* default_url =
      AddNewTemplateURL("http://d.com/", ASCIIToUTF16("d"));
  util_.model()->SetDefaultSearchProvider(default_url);

  // Wait for the changes to be saved.
  base::RunLoop().RunUntilIdle();

  // Verify the search providers install state (with no default set).
  test_get_install_state.RunTests(google_host, std::string());

  // Change the Google base url.
  google_host = "foo.com";
  util_.SetGoogleBaseURL(GURL("http://" + google_host + "/"));
  // Wait for the I/O thread to process the update notification.
  base::RunLoop().RunUntilIdle();

  // Verify that the change got picked up.
  test_get_install_state.RunTests(google_host, std::string());
}
