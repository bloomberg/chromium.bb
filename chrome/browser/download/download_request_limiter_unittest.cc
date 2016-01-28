// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_limiter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/download/download_request_infobar_delegate_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#else
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#endif

using content::WebContents;

class DownloadRequestLimiterTest;

#if !defined(OS_ANDROID)
class FakePermissionBubbleView : public PermissionBubbleView {
 public:
  class Factory : public base::RefCounted<FakePermissionBubbleView::Factory> {
   public:
    explicit Factory(DownloadRequestLimiterTest* test) : test_(test) {}

    scoped_ptr<PermissionBubbleView> Create(Browser* browser) {
      return make_scoped_ptr(new FakePermissionBubbleView(test_));
    }

   private:
    friend class base::RefCounted<FakePermissionBubbleView::Factory>;

    ~Factory() {}
    DownloadRequestLimiterTest* test_;
  };

  explicit FakePermissionBubbleView(DownloadRequestLimiterTest *test)
      : test_(test), delegate_(NULL) {}

  ~FakePermissionBubbleView() override {
  }

  void Close() {
    if (delegate_)
      delegate_->Closing();
  }

  // PermissionBubbleView:
  void SetDelegate(Delegate* delegate) override { delegate_ = delegate; }

  void Show(const std::vector<PermissionBubbleRequest*>& requests,
            const std::vector<bool>& accept_state) override;

  bool CanAcceptRequestUpdate() override { return false; }

  void Hide() override {}
  bool IsVisible() override { return false; }
  void UpdateAnchorPosition() override{};
  gfx::NativeWindow GetNativeWindow() override { return nullptr; }

 private:
  DownloadRequestLimiterTest* test_;
  Delegate* delegate_;
};
#endif

class DownloadRequestLimiterTest : public ChromeRenderViewHostTestHarness {
 public:
  enum TestingAction {
    ACCEPT,
    CANCEL,
    WAIT
  };

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_.reset(new TestingProfile());

#if !defined(OS_ANDROID)
    PermissionBubbleManager::CreateForWebContents(web_contents());
    scoped_refptr<FakePermissionBubbleView::Factory> factory =
        new FakePermissionBubbleView::Factory(this);
    PermissionBubbleManager::FromWebContents(web_contents())->view_factory_ =
        base::Bind(&FakePermissionBubbleView::Factory::Create, factory);
    PermissionBubbleManager::FromWebContents(web_contents())
        ->DisplayPendingRequests();
#endif

    testing_action_ = ACCEPT;
    ask_allow_count_ = cancel_count_ = continue_count_ = 0;
    download_request_limiter_ = new DownloadRequestLimiter();

#if defined(OS_ANDROID)
    InfoBarService::CreateForWebContents(web_contents());
    fake_create_callback_ = base::Bind(
        &DownloadRequestLimiterTest::FakeCreate, base::Unretained(this));
    DownloadRequestInfoBarDelegateAndroid::SetCallbackForTesting(
        &fake_create_callback_);
#endif

    content_settings_ = new HostContentSettingsMap(
        profile_->GetPrefs(), false /* incognito_profile */,
        false /* guest_profile */);
    DownloadRequestLimiter::SetContentSettingsForTesting(
        content_settings_.get());
  }

  int GetAction() {
    return testing_action_;
  }

  void AskAllow() {
    ask_allow_count_++;
  }

  void TearDown() override {
    content_settings_->ShutdownOnUIThread();
    content_settings_ = NULL;
#if defined(OS_ANDROID)
    UnsetDelegate();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

#if defined(OS_ANDROID)
  void FakeCreate(
      InfoBarService* infobar_service,
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host) {
    AskAllow();
    switch (testing_action_) {
      case ACCEPT:
        host->Accept();
        break;
      case CANCEL:
        host->Cancel();
        break;
      case WAIT:
        break;
    }
  }

  virtual void UnsetDelegate() {
    DownloadRequestInfoBarDelegateAndroid::SetCallbackForTesting(NULL);
  }
#endif

  void CanDownload() {
    CanDownloadFor(web_contents());
  }

  void CanDownloadFor(WebContents* web_contents) {
    download_request_limiter_->CanDownloadImpl(
        web_contents,
        "GET",  // request method
        base::Bind(&DownloadRequestLimiterTest::ContinueDownload,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
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

  void ExpectAndResetCounts(
      int expect_continues,
      int expect_cancels,
      int expect_asks,
      int line) {
    EXPECT_EQ(expect_continues, continue_count_) << "line " << line;
    EXPECT_EQ(expect_cancels, cancel_count_) << "line " << line;
    EXPECT_EQ(expect_asks, ask_allow_count_) << "line " << line;
    continue_count_ = cancel_count_ = ask_allow_count_ = 0;
  }

 protected:
  void ContinueDownload(bool allow) {
    if (allow) {
      continue_count_++;
    } else {
      cancel_count_++;
    }
  }

  void SetHostContentSetting(WebContents* contents, ContentSetting setting) {
    content_settings_->SetContentSetting(
        ContentSettingsPattern::FromURL(contents->GetURL()),
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
        std::string(),
        setting);
  }

  void BubbleManagerDocumentLoadCompleted() {
#if !defined(OS_ANDROID)
    PermissionBubbleManager::FromWebContents(web_contents())->
        DocumentOnLoadCompletedInMainFrame();
#endif
  }

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

  // The action that FakeCreate() should take.
  TestingAction testing_action_;

  // Number of times ContinueDownload was invoked.
  int continue_count_;

  // Number of times CancelDownload was invoked.
  int cancel_count_;

  // Number of times ShouldAllowDownload was invoked.
  int ask_allow_count_;

  scoped_refptr<HostContentSettingsMap> content_settings_;

 private:
#if defined(OS_ANDROID)
  DownloadRequestInfoBarDelegateAndroid::FakeCreateCallback
      fake_create_callback_;
#endif

  scoped_ptr<TestingProfile> profile_;
};

#if !defined(OS_ANDROID)
void FakePermissionBubbleView::Show(
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state) {
  test_->AskAllow();
  int action = test_->GetAction();
  if (action == DownloadRequestLimiterTest::ACCEPT) {
    delegate_->Accept();
  } else if (action == DownloadRequestLimiterTest::CANCEL) {
    delegate_->Deny();
  } else if (action == DownloadRequestLimiterTest::WAIT) {
    // do nothing.
  } else {
    delegate_->Closing();
  }
}
#endif

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_Allow) {
  BubbleManagerDocumentLoadCompleted();

  // All tabs should initially start at ALLOW_ONE_DOWNLOAD.
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask if the tab can do a download. This moves to PROMPT_BEFORE_DOWNLOAD.
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  // We should have been told we can download.
  ExpectAndResetCounts(1, 0, 0, __LINE__);

  // Ask again. This triggers asking the delegate for allow/disallow.
  testing_action_ = ACCEPT;
  CanDownload();
  // This should ask us if the download is allowed.
  // We should have been told we can download.
  ExpectAndResetCounts(1, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask again and make sure continue is invoked.
  CanDownload();
  // The state is at allow_all, which means the delegate shouldn't be asked.
  // We should have been told we can download.
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_ResetOnNavigation) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  BubbleManagerDocumentLoadCompleted();

  // Do two downloads, allowing the second so that we end up with allow all.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  testing_action_ = ACCEPT;
  CanDownload();
  ExpectAndResetCounts(1, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the allow
  // all state.
  NavigateAndCommit(GURL("http://foo.com/bar2"));
  BubbleManagerDocumentLoadCompleted();
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a user gesture, because we're at allow all, this shouldn't change the
  // state.
  OnUserGesture();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Navigate to a completely different host, which should reset the state.
  NavigateAndCommit(GURL("http://fooey.com"));
  BubbleManagerDocumentLoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do two downloads, allowing the second so that we end up with allow all.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  testing_action_ = CANCEL;
  CanDownload();
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the allow
  // all state.
  NavigateAndCommit(GURL("http://fooey.com/bar2"));
  BubbleManagerDocumentLoadCompleted();
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_ResetOnUserGesture) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  BubbleManagerDocumentLoadCompleted();

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a user gesture, which should reset back to allow one.
  OnUserGesture();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask twice, which triggers calling the delegate. Don't allow the download
  // so that we end up with not allowed.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  testing_action_ = CANCEL;
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  ExpectAndResetCounts(0, 1, 1, __LINE__);

  // A user gesture now should NOT change the state.
  OnUserGesture();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  // And make sure we really can't download.
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  // And the state shouldn't have changed.
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_ResetOnReload) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  BubbleManagerDocumentLoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // If the user refreshes the page without responding to the infobar, pretend
  // like the refresh is the initial load: they get 1 free download (probably
  // the same as the actual initial load), then an infobar.
  testing_action_ = WAIT;

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(0, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  Reload();
  BubbleManagerDocumentLoadCompleted();
  base::RunLoop().RunUntilIdle();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  ExpectAndResetCounts(1, 0, 0, __LINE__);

  testing_action_ = CANCEL;
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  ExpectAndResetCounts(0, 1, 1, __LINE__);

  Reload();
  BubbleManagerDocumentLoadCompleted();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

#if defined(OS_ANDROID)
TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_RawWebContents) {
  scoped_ptr<WebContents> web_contents(CreateTestWebContents());

  // DownloadRequestLimiter won't try to make a permission bubble if there's
  // no permission bubble manager, so don't put one on the test WebContents.

  // DownloadRequestLimiter won't try to make an infobar if it doesn't have an
  // InfoBarService, and we want to test that it will Cancel() instead of
  // prompting when it doesn't have a InfoBarService, so unset the delegate.
  UnsetDelegate();
  ExpectAndResetCounts(0, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  // You get one freebie.
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  OnUserGestureFor(web_contents.get());
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  OnUserGestureFor(web_contents.get());
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
}
#endif

TEST_F(DownloadRequestLimiterTest,
       DownloadRequestLimiter_SetHostContentSetting) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  BubbleManagerDocumentLoadCompleted();
  SetHostContentSetting(web_contents(), CONTENT_SETTING_ALLOW);

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  SetHostContentSetting(web_contents(), CONTENT_SETTING_BLOCK);

  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}
