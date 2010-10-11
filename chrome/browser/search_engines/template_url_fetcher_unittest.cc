// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_model_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

// Quits the current message loop as soon as there is a change to the
// TemplateURLModel.
class QuitOnChangedObserver : public TemplateURLModelObserver {
 public:
  explicit QuitOnChangedObserver(TemplateURLModel* model);
  virtual ~QuitOnChangedObserver();

  // TemplateURLModelObserver implemementation.
  virtual void OnTemplateURLModelChanged();

 private:
  TemplateURLModel* model_;
};

QuitOnChangedObserver::QuitOnChangedObserver(TemplateURLModel* model)
    : model_(model) {
  model_->AddObserver(this);
}

QuitOnChangedObserver::~QuitOnChangedObserver() {
  model_->RemoveObserver(this);
}

void QuitOnChangedObserver::OnTemplateURLModelChanged() {
  MessageLoop::current()->Quit();
}

// Basic set-up for TemplateURLFetcher tests.
class TemplateURLFetcherTest : public testing::Test {
 public:
  TemplateURLFetcherTest();

  virtual void SetUp() {
    ASSERT_TRUE(test_server_.Start());

    test_util_.SetUp();
    test_util_.StartIOThread();
    test_util_.profile()->CreateTemplateURLFetcher();
    ASSERT_TRUE(test_util_.profile()->GetTemplateURLFetcher());

    test_util_.profile()->CreateRequestContext();
    ASSERT_TRUE(test_util_.profile()->GetRequestContext());
  }

  virtual void TearDown() {
    test_util_.TearDown();
  }

 protected:
  // Schedules the download of the url.
  void StartDownload(const std::wstring& keyword,
                     const std::string& osdd_file_name,
                     TemplateURLFetcher::ProviderType provider_type,
                     bool check_that_file_exists);

  // Waits for any downloads to finish.
  void WaitForDownloadToFinish();

  TemplateURLModelTestUtil test_util_;
  net::TestServer test_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcherTest);
};

TemplateURLFetcherTest::TemplateURLFetcherTest()
    : test_server_(net::TestServer::TYPE_HTTP,
                   FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
}

void TemplateURLFetcherTest::StartDownload(
    const std::wstring& keyword,
    const std::string& osdd_file_name,
    TemplateURLFetcher::ProviderType provider_type,
    bool check_that_file_exists) {

  if (check_that_file_exists) {
    FilePath osdd_full_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &osdd_full_path));
    osdd_full_path = osdd_full_path.AppendASCII(osdd_file_name);
    ASSERT_TRUE(file_util::PathExists(osdd_full_path));
    ASSERT_FALSE(file_util::DirectoryExists(osdd_full_path));
  }

  // Start the fetch.
  GURL osdd_url = test_server_.GetURL("files/" + osdd_file_name);
  GURL favicon_url;
  test_util_.profile()->GetTemplateURLFetcher()->ScheduleDownload(
      keyword, osdd_url, favicon_url, NULL, provider_type);
  ASSERT_EQ(1, test_util_.profile()->GetTemplateURLFetcher()->requests_count());
}

void TemplateURLFetcherTest::WaitForDownloadToFinish() {
  QuitOnChangedObserver quit_on_changed_observer(test_util_.model());
  MessageLoop::current()->Run();
}

TEST_F(TemplateURLFetcherTest, BasicTest) {
  std::wstring keyword(L"test");

  test_util_.ChangeModelToLoadState();
  test_util_.ResetObserverCount();
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  WaitForDownloadToFinish();

  const TemplateURL* t_url = test_util_.model()->GetTemplateURLForKeyword(
      keyword);
  ASSERT_TRUE(t_url);
  EXPECT_STREQ(L"http://example.com/%s/other_stuff",
               t_url->url()->DisplayURL().c_str());
  EXPECT_EQ(true, t_url->safe_for_autoreplace());
}

TEST_F(TemplateURLFetcherTest, DuplicateThrownAway) {
  std::wstring keyword(L"test");

  test_util_.ChangeModelToLoadState();
  test_util_.ResetObserverCount();
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);

  struct {
    std::string description;
    std::string osdd_file_name;
    std::wstring keyword;
    TemplateURLFetcher::ProviderType provider_type;
  } test_cases[] = {
      { "Duplicate keyword and osdd url with autodetected provider.",
        osdd_file_name, keyword, TemplateURLFetcher::AUTODETECTED_PROVIDER },
      { "Duplicate keyword and osdd url with explicit provider.",
        osdd_file_name, keyword, TemplateURLFetcher::EXPLICIT_PROVIDER },
      { "Duplicate osdd url with explicit provider.",
        osdd_file_name, keyword + L"1", TemplateURLFetcher::EXPLICIT_PROVIDER },
      { "Duplicate keyword with explicit provider.",
        osdd_file_name + "1", keyword, TemplateURLFetcher::EXPLICIT_PROVIDER }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    StartDownload(test_cases[i].keyword, test_cases[i].osdd_file_name,
                  test_cases[i].provider_type, false);
    ASSERT_EQ(
        1,
        test_util_.profile()->GetTemplateURLFetcher()->requests_count()) <<
        test_cases[i].description;
  }

  WaitForDownloadToFinish();
}
