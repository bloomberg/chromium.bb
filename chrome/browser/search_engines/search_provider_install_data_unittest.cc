// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/search_provider_install_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

// Create a TemplateURL. The caller owns the returned TemplateURL*.
static TemplateURL* CreateTemplateURL(const std::string& url,
                                      const std::string& keyword) {
  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL(url, 0, 0);
  t_url->set_keyword(UTF8ToUTF16(keyword));
  t_url->set_short_name(UTF8ToUTF16(keyword));
  return t_url;
}

// Test the SearchProviderInstallData::GetInstallState.
class TestGetInstallState :
    public base::RefCountedThreadSafe<TestGetInstallState> {
 public:
  explicit TestGetInstallState(SearchProviderInstallData* install_data)
      : install_data_(install_data),
        main_loop_(NULL),
        passed_(false) {
  }

  void set_search_provider_host(
      const std::string& search_provider_host) {
    search_provider_host_ = search_provider_host;
  }

  void set_default_search_provider_host(
      const std::string& default_search_provider_host) {
    default_search_provider_host_ = default_search_provider_host;
  }

  // Runs the test. Returns true if all passed. False if any failed.
  bool RunTests();

 private:
  friend class base::RefCountedThreadSafe<TestGetInstallState>;
  ~TestGetInstallState();

  // Starts the test run on the IO thread.
  void StartTestOnIOThread();

  // Callback for when SearchProviderInstallData is ready to have
  // GetInstallState called. Runs all of the test cases.
  void DoInstallStateTests();

  // Does a verification for one url and its expected state.
  void VerifyInstallState(SearchProviderInstallData::State expected_state,
                          const std::string& url);

  SearchProviderInstallData* install_data_;
  MessageLoop* main_loop_;

  // A host which should be a search provider but not the default.
  std::string search_provider_host_;

  // A host which should be a search provider but not the default.
  std::string default_search_provider_host_;

  // Used to indicate if DoInstallStateTests passed all test.
  bool passed_;

  DISALLOW_COPY_AND_ASSIGN(TestGetInstallState);
};

bool TestGetInstallState::RunTests() {
  passed_ = true;

  main_loop_ = MessageLoop::current();

  BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)->PostTask(
      FROM_HERE,
      base::Bind(&TestGetInstallState::StartTestOnIOThread, this));
  // Run the current message loop. When the test is finished on the I/O thread,
  // it invokes Quit, which unblocks this.
  MessageLoop::current()->Run();
  main_loop_ = NULL;

  // Let the testing code know what the result is.
  return passed_;
}

TestGetInstallState::~TestGetInstallState() {
}

void TestGetInstallState::StartTestOnIOThread() {
  install_data_->CallWhenLoaded(
      base::Bind(&TestGetInstallState::DoInstallStateTests, this));
}

void TestGetInstallState::DoInstallStateTests() {
  // Installed but not default.
  VerifyInstallState(SearchProviderInstallData::INSTALLED_BUT_NOT_DEFAULT,
                     "http://" + search_provider_host_ + "/");
  VerifyInstallState(SearchProviderInstallData::INSTALLED_BUT_NOT_DEFAULT,
                     "http://" + search_provider_host_ + ":80/");

  // Not installed.
  VerifyInstallState(SearchProviderInstallData::NOT_INSTALLED,
                     "http://" + search_provider_host_ + ":96/");

  // Not installed due to different scheme.
  VerifyInstallState(SearchProviderInstallData::NOT_INSTALLED,
                     "https://" + search_provider_host_ + "/");

  // Not installed.
  VerifyInstallState(SearchProviderInstallData::NOT_INSTALLED,
                     "http://a" + search_provider_host_ + "/");

  // Installed as default.
  if (!default_search_provider_host_.empty()) {
    VerifyInstallState(SearchProviderInstallData::INSTALLED_AS_DEFAULT,
                       "http://" + default_search_provider_host_ + "/");
  }

  // All done.
  main_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

void TestGetInstallState::VerifyInstallState(
    SearchProviderInstallData::State expected_state,
    const std::string& url) {

  SearchProviderInstallData::State actual_state =
      install_data_->GetInstallState(GURL(url));
  if (expected_state == actual_state)
    return;

  passed_ = false;
  LOG(ERROR) << "GetInstallState for " << url << " failed. Expected " <<
      expected_state << ".  Actual " << actual_state << ".";
}

// Provides basic test set-up/tear-down functionality needed by all tests
// that use TemplateURLServiceTestUtil.
class SearchProviderInstallDataTest : public testing::Test {
 public:
  SearchProviderInstallDataTest()
      : install_data_(NULL) {}

  virtual void SetUp() {
    testing::Test::SetUp();
    util_.SetUp();
    util_.StartIOThread();
    install_data_ = new SearchProviderInstallData(
        util_.GetWebDataService(),
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::Source<SearchProviderInstallDataTest>(this));
  }

  virtual void TearDown() {
    BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)->PostTask(
        FROM_HERE,
        new DeleteTask<SearchProviderInstallData>(install_data_));
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

  void SimulateDefaultSearchIsManaged(const std::string& url) {
    ASSERT_FALSE(url.empty());
    TestingPrefService* service = util_.profile()->GetTestingPrefService();
    service->SetManagedPref(
        prefs::kDefaultSearchProviderEnabled,
        Value::CreateBooleanValue(true));
    service->SetManagedPref(
        prefs::kDefaultSearchProviderSearchURL,
        Value::CreateStringValue(url));
    service->SetManagedPref(
        prefs::kDefaultSearchProviderName,
        Value::CreateStringValue("managed"));
    // Clear the IDs that are not specified via policy.
    service->SetManagedPref(
        prefs::kDefaultSearchProviderID, new StringValue(""));
    service->SetManagedPref(
        prefs::kDefaultSearchProviderPrepopulateID, new StringValue(""));
    util_.model()->Observe(
        chrome::NOTIFICATION_PREF_CHANGED,
        content::Source<PrefService>(util_.profile()->GetTestingPrefService()),
        content::Details<std::string>(NULL));
  }

 protected:
  TemplateURLServiceTestUtil util_;

  // Provides the search provider install state on the I/O thread. It must be
  // deleted on the I/O thread, which is why it isn't a scoped_ptr.
  SearchProviderInstallData* install_data_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderInstallDataTest);
};

TEST_F(SearchProviderInstallDataTest, GetInstallState) {
  // Set up the database.
  util_.ChangeModelToLoadState();
  std::string host = "www.unittest.com";
  TemplateURL* t_url = CreateTemplateURL("http://" + host + "/path",
                                         "unittest");
  util_.model()->Add(t_url);

  // Wait for the changes to be saved.
  TemplateURLServiceTestUtil::BlockTillServiceProcessesRequests();

  // Verify the search providers install state (with no default set).
  scoped_refptr<TestGetInstallState> test_get_install_state(
      new TestGetInstallState(install_data_));
  test_get_install_state->set_search_provider_host(host);
  EXPECT_TRUE(test_get_install_state->RunTests());

  // Set-up a default and try it all one more time.
  std::string default_host = "www.mmm.com";
  TemplateURL* default_url = CreateTemplateURL("http://" + default_host + "/",
                                               "mmm");
  util_.model()->Add(default_url);
  util_.model()->SetDefaultSearchProvider(default_url);
  test_get_install_state->set_default_search_provider_host(default_host);
  EXPECT_TRUE(test_get_install_state->RunTests());
}


TEST_F(SearchProviderInstallDataTest, ManagedDefaultSearch) {
  // Set up the database.
  util_.ChangeModelToLoadState();
  std::string host = "www.unittest.com";
  TemplateURL* t_url = CreateTemplateURL("http://" + host + "/path",
                                         "unittest");
  util_.model()->Add(t_url);

  // Set a managed preference that establishes a default search provider.
  std::string host2 = "www.managedtest.com";
  std::string url2 = "http://" + host2 + "/p{searchTerms}";
  SimulateDefaultSearchIsManaged(url2);
  EXPECT_TRUE(util_.model()->is_default_search_managed());

  // Wait for the changes to be saved.
  util_.BlockTillServiceProcessesRequests();

  // Verify the search providers install state.  The default search should be
  // the managed one we previously set.
  scoped_refptr<TestGetInstallState> test_get_install_state(
      new TestGetInstallState(install_data_));
  test_get_install_state->set_search_provider_host(host);
  test_get_install_state->set_default_search_provider_host(host2);
  EXPECT_TRUE(test_get_install_state->RunTests());
}


TEST_F(SearchProviderInstallDataTest, GoogleBaseUrlChange) {
  scoped_refptr<TestGetInstallState> test_get_install_state(
      new TestGetInstallState(install_data_));

  // Set up the database.
  util_.ChangeModelToLoadState();
  std::string google_host = "w.com";
  util_.SetGoogleBaseURL("http://" + google_host + "/");
  // Wait for the I/O thread to process the update notification.
  TemplateURLServiceTestUtil::BlockTillIOThreadProcessesRequests();

  TemplateURL* t_url = CreateTemplateURL("{google:baseURL}?q={searchTerms}",
                                         "t");
  util_.model()->Add(t_url);
  TemplateURL* default_url = CreateTemplateURL("http://d.com/",
                                               "d");
  util_.model()->Add(default_url);
  util_.model()->SetDefaultSearchProvider(default_url);

  // Wait for the changes to be saved.
  TemplateURLServiceTestUtil::BlockTillServiceProcessesRequests();

  // Verify the search providers install state (with no default set).
  test_get_install_state->set_search_provider_host(google_host);
  EXPECT_TRUE(test_get_install_state->RunTests());

  // Change the Google base url.
  google_host = "foo.com";
  util_.SetGoogleBaseURL("http://" + google_host + "/");
  // Wait for the I/O thread to process the update notification.
  TemplateURLServiceTestUtil::BlockTillIOThreadProcessesRequests();

  // Verify that the change got picked up.
  test_get_install_state->set_search_provider_host(google_host);
  EXPECT_TRUE(test_get_install_state->RunTests());
}
