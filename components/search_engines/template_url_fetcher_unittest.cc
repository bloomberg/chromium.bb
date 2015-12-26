// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_fetcher.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

// Basic set-up for TemplateURLFetcher tests.
class TemplateURLFetcherTest : public testing::Test {
 public:
  TemplateURLFetcherTest();

  void SetUp() override {
    TestingProfile* profile = test_util_.profile();
    ASSERT_TRUE(profile->GetRequestContext());
    template_url_fetcher_.reset(new TemplateURLFetcher(
        test_util_.model(), profile->GetRequestContext()));

    ASSERT_TRUE(test_server_.Start());
  }

  void TearDown() override {
    ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
  }

  // Called when the callback is destroyed.
  void DestroyedCallback();

  // TemplateURLFetcherCallbacks implementation.  (Although not derived from
  // this class, this method handles those calls for the test.)
  void ConfirmAddSearchProvider(
      base::ScopedClosureRunner* callback_destruction_notifier,
      scoped_ptr<TemplateURL> template_url);

  // Schedules the download of the url.
  void StartDownload(const base::string16& keyword,
                     const std::string& osdd_file_name,
                     TemplateURLFetcher::ProviderType provider_type,
                     bool check_that_file_exists);

  // Waits for any downloads to finish.
  void WaitForDownloadToFinish();

  TemplateURLServiceTestUtil* test_util() { return &test_util_; }
  TemplateURLFetcher* template_url_fetcher() {
    return template_url_fetcher_.get();
  }
  const TemplateURL* last_callback_template_url() const {
    return last_callback_template_url_.get();
  }
  int callbacks_destroyed() const { return callbacks_destroyed_; }
  int add_provider_called() const { return add_provider_called_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;  // To set up BrowserThreads.
  TemplateURLServiceTestUtil test_util_;
  scoped_ptr<TemplateURLFetcher> template_url_fetcher_;
  net::EmbeddedTestServer test_server_;

  // The last TemplateURL to come from a callback.
  scoped_ptr<TemplateURL> last_callback_template_url_;

  // How many TemplateURLFetcherTestCallbacks have been destructed.
  int callbacks_destroyed_;

  // How many times ConfirmAddSearchProvider has been called.
  int add_provider_called_;

  // Is the code in WaitForDownloadToFinish in a message loop waiting for a
  // callback to finish?
  bool waiting_for_download_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcherTest);
};

bool GetTestDataDir(base::FilePath* dir) {
  if (!PathService::Get(base::DIR_SOURCE_ROOT, dir))
    return false;
  *dir = dir->AppendASCII("components")
             .AppendASCII("test")
             .AppendASCII("data")
             .AppendASCII("search_engines");
  return true;
}

TemplateURLFetcherTest::TemplateURLFetcherTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      callbacks_destroyed_(0),
      add_provider_called_(0),
      waiting_for_download_(false) {
  base::FilePath test_data_dir;
  CHECK(GetTestDataDir(&test_data_dir));
  test_server_.ServeFilesFromDirectory(test_data_dir);
}

void TemplateURLFetcherTest::DestroyedCallback() {
  callbacks_destroyed_++;
  if (waiting_for_download_)
    base::MessageLoop::current()->QuitWhenIdle();
}

void TemplateURLFetcherTest::ConfirmAddSearchProvider(
    base::ScopedClosureRunner* callback_destruction_notifier,
    scoped_ptr<TemplateURL> template_url) {
  last_callback_template_url_ = std::move(template_url);
  add_provider_called_++;
}

void TemplateURLFetcherTest::StartDownload(
    const base::string16& keyword,
    const std::string& osdd_file_name,
    TemplateURLFetcher::ProviderType provider_type,
    bool check_that_file_exists) {
  if (check_that_file_exists) {
    base::FilePath osdd_full_path;
    ASSERT_TRUE(GetTestDataDir(&osdd_full_path));
    osdd_full_path = osdd_full_path.AppendASCII(osdd_file_name);
    ASSERT_TRUE(base::PathExists(osdd_full_path));
    ASSERT_FALSE(base::DirectoryExists(osdd_full_path));
  }

  // Start the fetch.
  GURL osdd_url = test_server_.GetURL("/" + osdd_file_name);
  GURL favicon_url;
  base::ScopedClosureRunner* callback_destruction_notifier =
      new base::ScopedClosureRunner(
          base::Bind(&TemplateURLFetcherTest::DestroyedCallback,
                     base::Unretained(this)));

  template_url_fetcher_->ScheduleDownload(
      keyword, osdd_url, favicon_url,
      TemplateURLFetcher::URLFetcherCustomizeCallback(),
      base::Bind(&TemplateURLFetcherTest::ConfirmAddSearchProvider,
                 base::Unretained(this),
                 base::Owned(callback_destruction_notifier)),
      provider_type);
}

void TemplateURLFetcherTest::WaitForDownloadToFinish() {
  ASSERT_FALSE(waiting_for_download_);
  waiting_for_download_ = true;
  base::MessageLoop::current()->Run();
  waiting_for_download_ = false;
}

TEST_F(TemplateURLFetcherTest, BasicAutodetectedTest) {
  base::string16 keyword(ASCIIToUTF16("test"));

  test_util()->ChangeModelToLoadState();
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  ASSERT_EQ(0, add_provider_called());
  ASSERT_EQ(0, callbacks_destroyed());

  WaitForDownloadToFinish();
  ASSERT_EQ(0, add_provider_called());
  ASSERT_EQ(1, callbacks_destroyed());

  const TemplateURL* t_url = test_util()->model()->GetTemplateURLForKeyword(
      keyword);
  ASSERT_TRUE(t_url);
  EXPECT_EQ(ASCIIToUTF16("http://example.com/%s/other_stuff"),
            t_url->url_ref().DisplayURL(
                test_util()->model()->search_terms_data()));
  EXPECT_EQ(ASCIIToUTF16("Simple Search"), t_url->short_name());
  EXPECT_TRUE(t_url->safe_for_autoreplace());
}

TEST_F(TemplateURLFetcherTest, DuplicatesThrownAway) {
  base::string16 keyword(ASCIIToUTF16("test"));

  test_util()->ChangeModelToLoadState();
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  ASSERT_EQ(0, add_provider_called());
  ASSERT_EQ(0, callbacks_destroyed());

  struct {
    std::string description;
    std::string osdd_file_name;
    base::string16 keyword;
    TemplateURLFetcher::ProviderType provider_type;
  } test_cases[] = {
      { "Duplicate osdd url with autodetected provider.", osdd_file_name,
        keyword + ASCIIToUTF16("1"),
        TemplateURLFetcher::AUTODETECTED_PROVIDER },
      { "Duplicate keyword with autodetected provider.", osdd_file_name + "1",
        keyword, TemplateURLFetcher::AUTODETECTED_PROVIDER },
      { "Duplicate osdd url with explicit provider.", osdd_file_name,
        base::string16(), TemplateURLFetcher::EXPLICIT_PROVIDER },
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    StartDownload(test_cases[i].keyword, test_cases[i].osdd_file_name,
                  test_cases[i].provider_type, false);
    ASSERT_EQ(1, template_url_fetcher()->requests_count())
        << test_cases[i].description;
    ASSERT_EQ(i + 1, static_cast<size_t>(callbacks_destroyed()));
  }

  WaitForDownloadToFinish();
  ASSERT_EQ(1 + arraysize(test_cases),
            static_cast<size_t>(callbacks_destroyed()));
  ASSERT_EQ(0, add_provider_called());
}

TEST_F(TemplateURLFetcherTest, BasicExplicitTest) {
  base::string16 keyword(ASCIIToUTF16("test"));

  test_util()->ChangeModelToLoadState();
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_PROVIDER, true);
  ASSERT_EQ(0, add_provider_called());
  ASSERT_EQ(0, callbacks_destroyed());

  WaitForDownloadToFinish();
  ASSERT_EQ(1, add_provider_called());
  ASSERT_EQ(1, callbacks_destroyed());

  ASSERT_TRUE(last_callback_template_url());
  EXPECT_EQ(ASCIIToUTF16("http://example.com/%s/other_stuff"),
            last_callback_template_url()->url_ref().DisplayURL(
                test_util()->model()->search_terms_data()));
  EXPECT_EQ(ASCIIToUTF16("example.com"),
            last_callback_template_url()->keyword());
  EXPECT_FALSE(last_callback_template_url()->safe_for_autoreplace());
}

TEST_F(TemplateURLFetcherTest, AutodetectedBeforeLoadTest) {
  base::string16 keyword(ASCIIToUTF16("test"));
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  ASSERT_EQ(0, add_provider_called());
  ASSERT_EQ(1, callbacks_destroyed());
}

TEST_F(TemplateURLFetcherTest, ExplicitBeforeLoadTest) {
  base::string16 keyword(ASCIIToUTF16("test"));
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_PROVIDER, true);
  ASSERT_EQ(0, add_provider_called());
  ASSERT_EQ(0, callbacks_destroyed());

  WaitForDownloadToFinish();
  ASSERT_EQ(1, add_provider_called());
  ASSERT_EQ(1, callbacks_destroyed());

  ASSERT_TRUE(last_callback_template_url());
  EXPECT_EQ(ASCIIToUTF16("http://example.com/%s/other_stuff"),
            last_callback_template_url()->url_ref().DisplayURL(
                test_util()->model()->search_terms_data()));
  EXPECT_EQ(ASCIIToUTF16("example.com"),
            last_callback_template_url()->keyword());
  EXPECT_FALSE(last_callback_template_url()->safe_for_autoreplace());
}

TEST_F(TemplateURLFetcherTest, DuplicateKeywordsTest) {
  base::string16 keyword(ASCIIToUTF16("test"));
  TemplateURLData data;
  data.SetShortName(keyword);
  data.SetKeyword(keyword);
  data.SetURL("http://example.com/");
  test_util()->model()->Add(new TemplateURL(data));
  test_util()->ChangeModelToLoadState();

  ASSERT_TRUE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  // This should bail because the keyword already exists.
  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  ASSERT_EQ(0, add_provider_called());
  ASSERT_EQ(1, callbacks_destroyed());
  ASSERT_FALSE(last_callback_template_url());
}

TEST_F(TemplateURLFetcherTest, DuplicateDownloadTest) {
  base::string16 keyword(ASCIIToUTF16("test"));
  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_PROVIDER, true);
  ASSERT_EQ(0, add_provider_called());
  ASSERT_EQ(0, callbacks_destroyed());

  // This should bail because the keyword already has a pending download.
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_PROVIDER, true);
  ASSERT_EQ(0, add_provider_called());
  ASSERT_EQ(1, callbacks_destroyed());

  WaitForDownloadToFinish();
  ASSERT_EQ(1, add_provider_called());
  ASSERT_EQ(2, callbacks_destroyed());
  ASSERT_TRUE(last_callback_template_url());
}

TEST_F(TemplateURLFetcherTest, UnicodeTest) {
  base::string16 keyword(ASCIIToUTF16("test"));

  test_util()->ChangeModelToLoadState();
  ASSERT_FALSE(test_util()->model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("unicode_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  WaitForDownloadToFinish();
  const TemplateURL* t_url =
      test_util()->model()->GetTemplateURLForKeyword(keyword);
  EXPECT_EQ(base::UTF8ToUTF16("\xd1\x82\xd0\xb5\xd1\x81\xd1\x82"),
            t_url->short_name());
}
