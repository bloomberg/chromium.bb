// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_fetcher_callbacks.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_model_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class TemplateURLFetcherTest;

// Handles callbacks from TemplateURLFetcher.
class TemplateURLFetcherTestCallbacks : public TemplateURLFetcherCallbacks {
 public:
  explicit TemplateURLFetcherTestCallbacks(TemplateURLFetcherTest* test)
      : test_(test) {
  }
  virtual ~TemplateURLFetcherTestCallbacks();

  // TemplateURLFetcherCallbacks implementation.
  virtual void ConfirmSetDefaultSearchProvider(
      TemplateURL* template_url,
      TemplateURLModel* template_url_model);
  virtual void ConfirmAddSearchProvider(
      TemplateURL* template_url,
      Profile* profile);

 private:
  TemplateURLFetcherTest* test_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcherTestCallbacks);
};

// Basic set-up for TemplateURLFetcher tests.
class TemplateURLFetcherTest : public testing::Test {
 public:
  TemplateURLFetcherTest();

  virtual void SetUp() {
    test_util_.SetUp();
    test_util_.StartIOThread();
    ASSERT_TRUE(test_util_.profile());
    test_util_.profile()->CreateTemplateURLFetcher();
    ASSERT_TRUE(test_util_.profile()->GetTemplateURLFetcher());

    test_util_.profile()->CreateRequestContext();
    ASSERT_TRUE(test_util_.profile()->GetRequestContext());
    ASSERT_TRUE(test_server_.Start());
  }

  virtual void TearDown() {
    test_util_.TearDown();
  }

  // Called by ~TemplateURLFetcherTestCallbacks.
  void DestroyedCallback(TemplateURLFetcherTestCallbacks* callbacks);

  // TemplateURLFetcherCallbacks implementation.  (Although not derived from
  // this class, these methods handle those calls for the test.)
  virtual void ConfirmSetDefaultSearchProvider(
      TemplateURL* template_url,
      TemplateURLModel* template_url_model);
  virtual void ConfirmAddSearchProvider(
      TemplateURL* template_url,
      Profile* profile);

 protected:
  // Schedules the download of the url.
  void StartDownload(const string16& keyword,
                     const std::string& osdd_file_name,
                     TemplateURLFetcher::ProviderType provider_type,
                     bool check_that_file_exists);

  // Waits for any downloads to finish.
  void WaitForDownloadToFinish();

  TemplateURLModelTestUtil test_util_;
  net::TestServer test_server_;

  // The last TemplateURL to come from a callback.
  scoped_ptr<TemplateURL> last_callback_template_url_;

  // How many TemplateURLFetcherTestCallbacks have been destructed.
  int callbacks_destroyed_;

  // How many times ConfirmSetDefaultSearchProvider has been called.
  int set_default_called_;

  // How many times ConfirmAddSearchProvider has been called.
  int add_provider_called_;

  // Is the code in WaitForDownloadToFinish in a message loop waiting for a
  // callback to finish?
  bool waiting_for_download_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcherTest);
};

TemplateURLFetcherTestCallbacks::~TemplateURLFetcherTestCallbacks() {
  test_->DestroyedCallback(this);
}

void TemplateURLFetcherTestCallbacks::ConfirmSetDefaultSearchProvider(
    TemplateURL* template_url,
    TemplateURLModel* template_url_model) {
  test_->ConfirmSetDefaultSearchProvider(template_url, template_url_model);
}

void TemplateURLFetcherTestCallbacks::ConfirmAddSearchProvider(
    TemplateURL* template_url,
    Profile* profile) {
  test_->ConfirmAddSearchProvider(template_url, profile);
}

TemplateURLFetcherTest::TemplateURLFetcherTest()
    : test_server_(net::TestServer::TYPE_HTTP,
                   FilePath(FILE_PATH_LITERAL("chrome/test/data"))),
      callbacks_destroyed_(0),
      set_default_called_(0),
      add_provider_called_(0),
      waiting_for_download_(false) {
}

void TemplateURLFetcherTest::DestroyedCallback(
    TemplateURLFetcherTestCallbacks* callbacks) {
  callbacks_destroyed_++;
  if (waiting_for_download_)
    MessageLoop::current()->Quit();
}

void TemplateURLFetcherTest::ConfirmSetDefaultSearchProvider(
    TemplateURL* template_url,
    TemplateURLModel* template_url_model) {
  last_callback_template_url_.reset(template_url);
  set_default_called_++;
}

void TemplateURLFetcherTest::ConfirmAddSearchProvider(
    TemplateURL* template_url,
    Profile* profile) {
  last_callback_template_url_.reset(template_url);
  add_provider_called_++;
}

void TemplateURLFetcherTest::StartDownload(
    const string16& keyword,
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
      keyword, osdd_url, favicon_url, new TemplateURLFetcherTestCallbacks(this),
      provider_type);
}

void TemplateURLFetcherTest::WaitForDownloadToFinish() {
  ASSERT_FALSE(waiting_for_download_);
  waiting_for_download_ = true;
  MessageLoop::current()->Run();
  waiting_for_download_ = false;
}

TEST_F(TemplateURLFetcherTest, BasicAutodetectedTest) {
  string16 keyword(ASCIIToUTF16("test"));

  test_util_.ChangeModelToLoadState();
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(0, callbacks_destroyed_);

  WaitForDownloadToFinish();
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(1, callbacks_destroyed_);

  const TemplateURL* t_url = test_util_.model()->GetTemplateURLForKeyword(
      keyword);
  ASSERT_TRUE(t_url);
  EXPECT_EQ(ASCIIToUTF16("http://example.com/%s/other_stuff"),
            t_url->url()->DisplayURL());
  EXPECT_TRUE(t_url->safe_for_autoreplace());
}

TEST_F(TemplateURLFetcherTest, DuplicatesThrownAway) {
  string16 keyword(ASCIIToUTF16("test"));

  test_util_.ChangeModelToLoadState();
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(0, callbacks_destroyed_);

  struct {
    std::string description;
    std::string osdd_file_name;
    string16 keyword;
    TemplateURLFetcher::ProviderType provider_type;
  } test_cases[] = {
      { "Duplicate keyword and osdd url with autodetected provider.",
        osdd_file_name, keyword, TemplateURLFetcher::AUTODETECTED_PROVIDER },
      { "Duplicate keyword and osdd url with explicit provider.",
        osdd_file_name, keyword, TemplateURLFetcher::EXPLICIT_PROVIDER },
      { "Duplicate osdd url with explicit provider.",
        osdd_file_name, keyword + ASCIIToUTF16("1"),
        TemplateURLFetcher::EXPLICIT_PROVIDER },
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
    ASSERT_EQ(i + 1, static_cast<size_t>(callbacks_destroyed_));
  }

  WaitForDownloadToFinish();
  ASSERT_EQ(1 + ARRAYSIZE_UNSAFE(test_cases),
            static_cast<size_t>(callbacks_destroyed_));
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
}

TEST_F(TemplateURLFetcherTest, BasicExplicitTest) {
  string16 keyword(ASCIIToUTF16("test"));

  test_util_.ChangeModelToLoadState();
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(0, callbacks_destroyed_);

  WaitForDownloadToFinish();
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(1, add_provider_called_);
  ASSERT_EQ(1, callbacks_destroyed_);

  ASSERT_TRUE(last_callback_template_url_.get());
  EXPECT_EQ(ASCIIToUTF16("http://example.com/%s/other_stuff"),
            last_callback_template_url_->url()->DisplayURL());
  EXPECT_FALSE(last_callback_template_url_->safe_for_autoreplace());
}

TEST_F(TemplateURLFetcherTest, BasicExplicitDefaultTest) {
  string16 keyword(ASCIIToUTF16("test"));

  test_util_.ChangeModelToLoadState();
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_DEFAULT_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(0, callbacks_destroyed_);

  WaitForDownloadToFinish();
  ASSERT_EQ(1, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(1, callbacks_destroyed_);

  ASSERT_TRUE(last_callback_template_url_.get());
  EXPECT_EQ(ASCIIToUTF16("http://example.com/%s/other_stuff"),
            last_callback_template_url_->url()->DisplayURL());
  EXPECT_FALSE(last_callback_template_url_->safe_for_autoreplace());
}

TEST_F(TemplateURLFetcherTest, AutodetectedBeforeLoadTest) {
  string16 keyword(ASCIIToUTF16("test"));
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(1, callbacks_destroyed_);
}

TEST_F(TemplateURLFetcherTest, ExplicitBeforeLoadTest) {
  string16 keyword(ASCIIToUTF16("test"));
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(1, callbacks_destroyed_);
}

TEST_F(TemplateURLFetcherTest, ExplicitDefaultBeforeLoadTest) {
  string16 keyword(ASCIIToUTF16("test"));
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_DEFAULT_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(0, callbacks_destroyed_);

  WaitForDownloadToFinish();
  ASSERT_EQ(1, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(1, callbacks_destroyed_);

  ASSERT_TRUE(last_callback_template_url_.get());
  EXPECT_EQ(ASCIIToUTF16("http://example.com/%s/other_stuff"),
            last_callback_template_url_->url()->DisplayURL());
  EXPECT_FALSE(last_callback_template_url_->safe_for_autoreplace());
}

TEST_F(TemplateURLFetcherTest, DuplicateKeywordsTest) {
  string16 keyword(ASCIIToUTF16("test"));

  TemplateURL* t_url = new TemplateURL();
  t_url->SetURL("http://example.com/", 0, 0);
  t_url->set_keyword(keyword);
  t_url->set_short_name(keyword);
  test_util_.model()->Add(t_url);
  test_util_.ChangeModelToLoadState();

  ASSERT_TRUE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  std::string osdd_file_name("simple_open_search.xml");
  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::AUTODETECTED_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(1, callbacks_destroyed_);

  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(2, callbacks_destroyed_);

  StartDownload(keyword, osdd_file_name,
                TemplateURLFetcher::EXPLICIT_DEFAULT_PROVIDER, true);
  ASSERT_EQ(0, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(2, callbacks_destroyed_);

  WaitForDownloadToFinish();
  ASSERT_EQ(1, set_default_called_);
  ASSERT_EQ(0, add_provider_called_);
  ASSERT_EQ(3, callbacks_destroyed_);
  ASSERT_TRUE(last_callback_template_url_.get());
  ASSERT_NE(keyword, last_callback_template_url_->keyword());
}
