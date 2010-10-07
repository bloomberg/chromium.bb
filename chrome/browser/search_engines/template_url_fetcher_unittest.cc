// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_thread.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_model_test_util.h"
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

    io_thread_.reset(new BrowserThread(BrowserThread::IO));
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_->StartWithOptions(options);

    test_util_.profile()->CreateTemplateURLFetcher();
    ASSERT_TRUE(test_util_.profile()->GetTemplateURLFetcher());

    test_util_.profile()->CreateRequestContext();
    ASSERT_TRUE(test_util_.profile()->GetRequestContext());
  }

  virtual void TearDown() {
    io_thread_->Stop();
    io_thread_.reset();
    test_util_.TearDown();
  }

 protected:
  TemplateURLModelTestUtil test_util_;
  net::TestServer test_server_;
  scoped_ptr<BrowserThread> io_thread_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcherTest);
};

TemplateURLFetcherTest::TemplateURLFetcherTest()
    : test_server_(net::TestServer::TYPE_HTTP,
                   FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
}

TEST_F(TemplateURLFetcherTest, BasicTest) {
  std::wstring keyword(L"test");

  test_util_.ChangeModelToLoadState();
  test_util_.ResetObserverCount();
  ASSERT_FALSE(test_util_.model()->GetTemplateURLForKeyword(keyword));

  GURL osdd_url = test_server_.GetURL("files/osdd/dictionary.xml");
  GURL favicon_url;
  test_util_.profile()->GetTemplateURLFetcher()->ScheduleDownload(
      keyword, osdd_url, favicon_url, NULL, true);

  {  // Wait for the url to be downloaded.
    QuitOnChangedObserver quit_on_changed_observer(test_util_.model());
    MessageLoop::current()->Run();
  }

  const TemplateURL* t_url = test_util_.model()->GetTemplateURLForKeyword(
      keyword);
  ASSERT_TRUE(t_url);
  EXPECT_STREQ(L"http://dictionary.reference.com/browse/%s?r=75",
               t_url->url()->DisplayURL().c_str());
  EXPECT_EQ(true, t_url->safe_for_autoreplace());
}

