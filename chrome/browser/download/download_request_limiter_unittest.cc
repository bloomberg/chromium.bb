// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::WebContents;

class DownloadRequestLimiterTest : public TabContentsTestHarness {
 public:
  DownloadRequestLimiterTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_user_blocking_thread_(
            BrowserThread::FILE_USER_BLOCKING, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

  virtual void SetUp() {
    TabContentsTestHarness::SetUp();

    allow_download_ = true;
    ask_allow_count_ = cancel_count_ = continue_count_ = 0;

    download_request_limiter_ = new DownloadRequestLimiter();
    test_delegate_.reset(new DownloadRequestLimiterTestDelegate(this));
    DownloadRequestLimiter::SetTestingDelegate(test_delegate_.get());
  }

  virtual void TearDown() {
    UnsetDelegate();
    TabContentsTestHarness::TearDown();
  }

  virtual void UnsetDelegate() {
    DownloadRequestLimiter::SetTestingDelegate(NULL);
  }

  void CanDownload() {
    CanDownloadFor(web_contents());
  }

  void CanDownloadFor(WebContents* web_contents) {
    download_request_limiter_->CanDownloadImpl(
        web_contents,
        -1,  // request id
        "GET",  // request method
        base::Bind(&DownloadRequestLimiterTest::ContinueDownload,
                   base::Unretained(this)));
    message_loop_.RunAllPending();
  }

  void OnUserGesture() {
    OnUserGestureFor(web_contents());
  }

  void OnUserGestureFor(WebContents* web_contents) {
    DownloadRequestLimiter::TabDownloadState* state =
        download_request_limiter_->GetDownloadState(web_contents, NULL, false);
    if (state)
      state->DidGetUserGesture();
  }

  bool ShouldAllowDownload() {
    ask_allow_count_++;
    return allow_download_;
  }

 protected:
  void ContinueDownload(bool allow) {
    if (allow) {
      continue_count_++;
    } else {
      cancel_count_++;
    }
  }

  class DownloadRequestLimiterTestDelegate
      : public DownloadRequestLimiter::TestingDelegate {
   public:
    explicit DownloadRequestLimiterTestDelegate(
        DownloadRequestLimiterTest* test)
        : test_(test) { }

    virtual bool ShouldAllowDownload() {
      return test_->ShouldAllowDownload();
    }

   private:
    DownloadRequestLimiterTest* test_;
  };

  scoped_ptr<DownloadRequestLimiterTestDelegate> test_delegate_;
  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

  // Number of times ContinueDownload was invoked.
  int continue_count_;

  // Number of times CancelDownload was invoked.
  int cancel_count_;

  // Whether the download should be allowed.
  bool allow_download_;

  // Number of times ShouldAllowDownload was invoked.
  int ask_allow_count_;

  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_user_blocking_thread_;
  content::TestBrowserThread io_thread_;
};

TEST_F(DownloadRequestLimiterTest,
       DownloadRequestLimiter_Allow) {
  // All tabs should initially start at ALLOW_ONE_DOWNLOAD.
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(contents()));

  // Ask if the tab can do a download. This moves to PROMPT_BEFORE_DOWNLOAD.
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(contents()));
  // We should have been told we can download.
  ASSERT_EQ(1, continue_count_);
  ASSERT_EQ(0, cancel_count_);
  ASSERT_EQ(0, ask_allow_count_);
  continue_count_ = 0;

  // Ask again. This triggers asking the delegate for allow/disallow.
  allow_download_ = true;
  CanDownload();
  // This should ask us if the download is allowed.
  ASSERT_EQ(1, ask_allow_count_);
  ask_allow_count_ = 0;
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(contents()));
  // We should have been told we can download.
  ASSERT_EQ(1, continue_count_);
  ASSERT_EQ(0, cancel_count_);
  continue_count_ = 0;

  // Ask again and make sure continue is invoked.
  CanDownload();
  // The state is at allow_all, which means the delegate shouldn't be asked.
  ASSERT_EQ(0, ask_allow_count_);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(contents()));
  // We should have been told we can download.
  ASSERT_EQ(1, continue_count_);
  ASSERT_EQ(0, cancel_count_);
  continue_count_ = 0;
}

TEST_F(DownloadRequestLimiterTest,
       DownloadRequestLimiter_ResetOnNavigation) {
  NavigateAndCommit(GURL("http://foo.com/bar"));

  // Do two downloads, allowing the second so that we end up with allow all.
  CanDownload();
  allow_download_ = true;
  CanDownload();
  ask_allow_count_ = continue_count_ = cancel_count_ = 0;
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the allow
  // all state.
  NavigateAndCommit(GURL("http://foo.com/bar2"));
  CanDownload();
  ASSERT_EQ(1, continue_count_);
  ASSERT_EQ(0, cancel_count_);
  ASSERT_EQ(0, ask_allow_count_);
  ask_allow_count_ = continue_count_ = cancel_count_ = 0;
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(contents()));

  // Do a user gesture, because we're at allow all, this shouldn't change the
  // state.
  OnUserGesture();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(contents()));

  // Navigate to a completely different host, which should reset the state.
  NavigateAndCommit(GURL("http://fooey.com"));
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(contents()));
}

TEST_F(DownloadRequestLimiterTest,
       DownloadRequestLimiter_ResetOnUserGesture) {
  NavigateAndCommit(GURL("http://foo.com/bar"));

  // Do one download, which should change to prompt before download.
  CanDownload();
  ask_allow_count_ = continue_count_ = cancel_count_ = 0;
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(contents()));

  // Do a user gesture, which should reset back to allow one.
  OnUserGesture();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(contents()));

  // Ask twice, which triggers calling the delegate. Don't allow the download
  // so that we end up with not allowed.
  allow_download_ = false;
  CanDownload();
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(contents()));

  // A user gesture now should NOT change the state.
  OnUserGesture();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(contents()));
  // And make sure we really can't download.
  ask_allow_count_ = continue_count_ = cancel_count_ = 0;
  CanDownload();
  ASSERT_EQ(0, ask_allow_count_);
  ASSERT_EQ(0, continue_count_);
  ASSERT_EQ(1, cancel_count_);
  // And the state shouldn't have changed.
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(contents()));
}

TEST_F(DownloadRequestLimiterTest,
       DownloadRequestLimiter_RawWebContents) {
  // By-pass TabContentsTestHarness and use
  // RenderViewHostTestHarness::CreateTestWebContents() directly so that there
  // will be no TabContents for web_contents.
  scoped_ptr<WebContents> web_contents(CreateTestWebContents());
  TabContents* tab_contents = TabContents::FromWebContents(web_contents.get());
  ASSERT_TRUE(tab_contents == NULL);
  // DownloadRequestLimiter won't try to make an infobar if it doesn't have a
  // TabContents, and we want to test that it will Cancel() instead of prompting
  // when it doesn't have a TabContents, so unset the delegate.
  UnsetDelegate();
  EXPECT_EQ(0, continue_count_);
  EXPECT_EQ(0, cancel_count_);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  // You get one freebie.
  CanDownloadFor(web_contents.get());
  EXPECT_EQ(1, continue_count_);
  EXPECT_EQ(0, cancel_count_);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  OnUserGestureFor(web_contents.get());
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  EXPECT_EQ(2, continue_count_);
  EXPECT_EQ(0, cancel_count_);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  EXPECT_EQ(2, continue_count_);
  EXPECT_EQ(1, cancel_count_);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  OnUserGestureFor(web_contents.get());
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  EXPECT_EQ(3, continue_count_);
  EXPECT_EQ(1, cancel_count_);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
}
