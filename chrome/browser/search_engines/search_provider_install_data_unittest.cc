// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/search_provider_install_data.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/mock_render_process_host.h"
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

 protected:
  TemplateURL* AddNewTemplateURL(const std::string& url,
                                 const base::string16& keyword);

  // Sets the Google base URL to |base_url| and runs the IO thread for
  // |SearchProviderInstallData| to process the update.
  void SetGoogleBaseURLAndProcessOnIOThread(GURL base_url);

  TemplateURLServiceTestUtil util_;

  // Provides the search provider install state on the I/O thread. It must be
  // deleted on the I/O thread, which is why it isn't a scoped_ptr.
  SearchProviderInstallData* install_data_;

  // A mock RenderProcessHost that the SearchProviderInstallData will scope its
  // lifetime to.
  scoped_ptr<content::MockRenderProcessHost> process_;

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
  process_.reset(new content::MockRenderProcessHost(util_.profile()));
  install_data_ = new SearchProviderInstallData(
      util_.model(), SearchTermsData().GoogleBaseURLValue(), NULL,
      process_.get());
}

void SearchProviderInstallDataTest::TearDown() {
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, install_data_);
  install_data_ = NULL;

  // Make sure that the install data class on the UI thread gets cleaned up.
  // It doesn't matter that this happens after install_data_ is deleted.
  process_.reset();

  testing::Test::TearDown();
}

TemplateURL* SearchProviderInstallDataTest::AddNewTemplateURL(
    const std::string& url,
    const base::string16& keyword) {
  TemplateURLData data;
  data.short_name = keyword;
  data.SetKeyword(keyword);
  data.SetURL(url);
  TemplateURL* t_url = new TemplateURL(data);
  util_.model()->Add(t_url);
  return t_url;
}

void SearchProviderInstallDataTest::SetGoogleBaseURLAndProcessOnIOThread(
    GURL base_url) {
  util_.SetGoogleBaseURL(base_url);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SearchProviderInstallData::OnGoogleURLChange,
                 base::Unretained(install_data_),
                 base_url.spec()));

  // Wait for the I/O thread to process the update notification.
  base::RunLoop().RunUntilIdle();
}

// Actual tests ---------------------------------------------------------------

TEST_F(SearchProviderInstallDataTest, GetInstallState) {
  // Set up the database.
  util_.ChangeModelToLoadState();
  std::string host = "www.unittest.com";
  AddNewTemplateURL("http://" + host + "/path", base::ASCIIToUTF16("unittest"));

  // Wait for the changes to be saved.
  base::RunLoop().RunUntilIdle();

  // Verify the search providers install state (with no default set).
  TestGetInstallState test_get_install_state(install_data_);
  test_get_install_state.RunTests(host, std::string());

  // Set-up a default and try it all one more time.
  std::string default_host = "www.mmm.com";
  TemplateURL* default_url =
      AddNewTemplateURL("http://" + default_host + "/",
                        base::ASCIIToUTF16("mmm"));
  util_.model()->SetUserSelectedDefaultSearchProvider(default_url);
  test_get_install_state.RunTests(host, default_host);
}

TEST_F(SearchProviderInstallDataTest, ManagedDefaultSearch) {
  // Set up the database.
  util_.ChangeModelToLoadState();
  std::string host = "www.unittest.com";
  AddNewTemplateURL("http://" + host + "/path", base::ASCIIToUTF16("unittest"));

  // Set a managed preference that establishes a default search provider.
  std::string host2 = "www.managedtest.com";
  util_.SetManagedDefaultSearchPreferences(
      true,
      "managed",
      "managed",
      "http://" + host2 + "/p{searchTerms}",
      std::string(),
      std::string(),
      std::string(),
      std::string(),
      std::string());

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
  SetGoogleBaseURLAndProcessOnIOThread(GURL("http://" + google_host + "/"));

  AddNewTemplateURL("{google:baseURL}?q={searchTerms}",
                    base::ASCIIToUTF16("t"));
  TemplateURL* default_url =
      AddNewTemplateURL("http://d.com/", base::ASCIIToUTF16("d"));
  util_.model()->SetUserSelectedDefaultSearchProvider(default_url);

  // Wait for the changes to be saved.
  base::RunLoop().RunUntilIdle();

  // Verify the search providers install state (with no default set).
  test_get_install_state.RunTests(google_host, std::string());

  // Change the Google base url.
  google_host = "foo.com";
  SetGoogleBaseURLAndProcessOnIOThread(GURL("http://" + google_host + "/"));

  // Verify that the change got picked up.
  test_get_install_state.RunTests(google_host, std::string());
}
