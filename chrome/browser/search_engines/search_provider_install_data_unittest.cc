// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/search_engines/search_provider_install_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_model_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Create a TemplateURL. The caller owns the returned TemplateURL*.
static TemplateURL* CreateTemplateURL(const std::string& url,
                                      const std::wstring& keyword) {
  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL(url, 0, 0);
  t_url->set_keyword(keyword);
  t_url->set_short_name(keyword);
  return t_url;
}

// Test the SearchProviderInstallData::GetInstallState.
class TestGetInstallState :
    public base::RefCountedThreadSafe<TestGetInstallState> {
 public:
  explicit TestGetInstallState(WebDataService* web_data_service)
      : install_data_(web_data_service),
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
  bool RunTests(const ChromeThread& io_thread);

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

  SearchProviderInstallData install_data_;
  MessageLoop* main_loop_;

  // A host which should be a search provider but not the default.
  std::string search_provider_host_;

  // A host which should be a search provider but not the default.
  std::string default_search_provider_host_;

  // Used to indicate if DoInstallStateTests passed all test.
  bool passed_;

  DISALLOW_COPY_AND_ASSIGN(TestGetInstallState);
};

bool TestGetInstallState::RunTests(const ChromeThread& io_thread) {
  passed_ = true;

  main_loop_ = MessageLoop::current();

  io_thread.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &TestGetInstallState::StartTestOnIOThread));
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
  install_data_.CallWhenLoaded(
      NewRunnableMethod(this,
                        &TestGetInstallState::DoInstallStateTests));
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
      install_data_.GetInstallState(GURL(url));
  if (expected_state == actual_state)
    return;

  passed_ = false;
  LOG(ERROR) << "GetInstallState for " << url << " failed. Expected " <<
      expected_state << ".  Actual " << actual_state << ".";
}

// Provides basic test set-up/tear-down functionality needed by all tests
// that use TemplateURLModelTestUtil.
class SearchProviderInstallDataTest : public testing::Test {
 public:
  SearchProviderInstallDataTest() {}

  virtual void SetUp() {
    testing::Test::SetUp();
    util_.SetUp();
    io_thread_.reset(new ChromeThread(ChromeThread::IO));
    io_thread_->Start();
  }

  virtual void TearDown() {
    io_thread_->Stop();
    io_thread_.reset();
    util_.TearDown();
    testing::Test::TearDown();
  }

 protected:
  TemplateURLModelTestUtil util_;
  scoped_ptr<ChromeThread> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderInstallDataTest);
};

TEST_F(SearchProviderInstallDataTest, GetInstallState) {
  // Set up the database.
  util_.ChangeModelToLoadState();
  std::string host = "www.unittest.com";
  TemplateURL* t_url = CreateTemplateURL("http://" + host + "/path",
                                         L"unittest");
  util_.model()->Add(t_url);

  // Wait for the changes to be saved.
  util_.BlockTillServiceProcessesRequests();

  // Verify the search providers install state (with no default set).
  scoped_refptr<TestGetInstallState> test_get_install_state(
      new TestGetInstallState(util_.GetWebDataService()));
  test_get_install_state->set_search_provider_host(host);
  EXPECT_TRUE(test_get_install_state->RunTests(*io_thread_.get()));

  // Set-up a default and try it all one more time.
  std::string default_host = "www.mmm.com";
  TemplateURL* default_url = CreateTemplateURL("http://" + default_host + "/",
                                               L"mmm");
  util_.model()->Add(default_url);
  util_.model()->SetDefaultSearchProvider(default_url);
  test_get_install_state->set_default_search_provider_host(default_host);
  EXPECT_TRUE(test_get_install_state->RunTests(*io_thread_.get()));
}
