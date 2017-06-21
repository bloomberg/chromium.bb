// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_limiter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/download/download_request_infobar_delegate_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#else
#endif

using content::WebContents;

namespace {
enum TestingAction {
  ACCEPT,
  CANCEL,
  WAIT
};
}  // namespace

class DownloadRequestLimiterTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    use_permission_request_manager_ = PermissionRequestManager::IsEnabled();

    if (use_permission_request_manager_) {
      PermissionRequestManager::CreateForWebContents(web_contents());
      PermissionRequestManager* manager =
          PermissionRequestManager::FromWebContents(web_contents());
      mock_permission_prompt_factory_.reset(
          new MockPermissionPromptFactory(manager));
      manager->DisplayPendingRequests();
    } else {
#if defined(OS_ANDROID)
      InfoBarService::CreateForWebContents(web_contents());
      fake_create_callback_ = base::Bind(
          &DownloadRequestLimiterTest::FakeCreate, base::Unretained(this));
      DownloadRequestInfoBarDelegateAndroid::SetCallbackForTesting(
          &fake_create_callback_);
#endif
    }

    UpdateExpectations(ACCEPT);
    ask_allow_count_ = cancel_count_ = continue_count_ = 0;
    download_request_limiter_ = new DownloadRequestLimiter();
  }

  void TearDown() override {
    if (use_permission_request_manager_) {
      mock_permission_prompt_factory_.reset();
    } else {
#if defined(OS_ANDROID)
      UnsetInfobarDelegate();
#endif
    }

    ChromeRenderViewHostTestHarness::TearDown();
  }

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

  void OnUserInteraction(blink::WebInputEvent::Type type) {
    OnUserInteractionFor(web_contents(), type);
  }

  void OnUserInteractionFor(WebContents* web_contents,
                            blink::WebInputEvent::Type type) {
    DownloadRequestLimiter::TabDownloadState* state =
        download_request_limiter_->GetDownloadState(web_contents, nullptr,
                                                    false);
    if (state)
      state->DidGetUserInteraction(type);
  }

  void ExpectAndResetCounts(
      int expect_continues,
      int expect_cancels,
      int expect_asks,
      int line) {
    EXPECT_EQ(expect_continues, continue_count_) << "line " << line;
    EXPECT_EQ(expect_cancels, cancel_count_) << "line " << line;
    EXPECT_EQ(expect_asks, AskAllowCount()) << "line " << line;
    continue_count_ = cancel_count_ = 0;
    if (use_permission_request_manager_)
      mock_permission_prompt_factory_->ResetCounts();
    else
      ask_allow_count_ = 0;
  }

  void UpdateContentSettings(WebContents* web_contents,
                             ContentSetting setting) {
    // Ensure a download state exists.
    download_request_limiter_->GetDownloadState(web_contents, nullptr, true);
    SetHostContentSetting(web_contents, setting);
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
    HostContentSettingsMapFactory::GetForProfile(
        Profile::FromBrowserContext(contents->GetBrowserContext()))
        ->SetContentSettingDefaultScope(
            contents->GetURL(), GURL(),
            CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, std::string(), setting);
  }

  void LoadCompleted() {
    if (use_permission_request_manager_)
      mock_permission_prompt_factory_->DocumentOnLoadCompletedInMainFrame();
  }

  int AskAllowCount() {
    if (use_permission_request_manager_)
      return mock_permission_prompt_factory_->show_count();
    return ask_allow_count_;
  }

  void UpdateExpectations(TestingAction action) {
    if (use_permission_request_manager_) {
      // Set expectations for PermissionRequestManager.
      PermissionRequestManager::AutoResponseType response_type =
          PermissionRequestManager::DISMISS;
      switch (action) {
        case ACCEPT:
          response_type = PermissionRequestManager::ACCEPT_ALL;
          break;
        case CANCEL:
          response_type = PermissionRequestManager::DENY_ALL;
          break;
        case WAIT:
          response_type = PermissionRequestManager::NONE;
          break;
      }
      mock_permission_prompt_factory_->set_response_type(response_type);
    } else {
      testing_action_ = action;
    }
  }

#if defined(OS_ANDROID)
  void FakeCreate(
      InfoBarService* infobar_service,
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host) {
    ask_allow_count_++;
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

  void UnsetInfobarDelegate() {
    if (!use_permission_request_manager_)
      DownloadRequestInfoBarDelegateAndroid::SetCallbackForTesting(nullptr);
  }
#endif

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

  // Number of times ContinueDownload was invoked.
  int continue_count_;

  // Number of times CancelDownload was invoked.
  int cancel_count_;

  std::unique_ptr<MockPermissionPromptFactory> mock_permission_prompt_factory_;

  // TODO(timloh): Remove the members below here after crbug.com/606138 is done.

  bool use_permission_request_manager_;

  // Number of times ShouldAllowDownload was invoked.
  int ask_allow_count_;
  // The action that FakeCreate() should take.
  TestingAction testing_action_;
#if defined(OS_ANDROID)
  DownloadRequestInfoBarDelegateAndroid::FakeCreateCallback
      fake_create_callback_;
#endif
};

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_Allow) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

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
  UpdateExpectations(ACCEPT);
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
  LoadCompleted();

  // Do two downloads, allowing the second so that we end up with allow all.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  UpdateExpectations(ACCEPT);
  CanDownload();
  ExpectAndResetCounts(1, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the allow
  // all state.
  NavigateAndCommit(GURL("http://foo.com/bar2"));
  LoadCompleted();
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a user gesture, because we're at allow all, this shouldn't change the
  // state.
  OnUserInteraction(blink::WebInputEvent::kRawKeyDown);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Navigate to a completely different host, which should reset the state.
  NavigateAndCommit(GURL("http://fooey.com"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do two downloads, blocking the second so that we end up with downloads not
  // allowed.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  UpdateExpectations(CANCEL);
  CanDownload();
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the allow
  // all state.
  NavigateAndCommit(GURL("http://fooey.com/bar2"));
  LoadCompleted();
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_RendererInitiated) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Do one download so we end up in PROMPT.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set up a renderer-initiated navigation to the same host.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame());
  rfh_tester->NavigateAndCommitRendererInitiated(true,
                                                 GURL("http://foo.com/bar2"));
  LoadCompleted();

  // The state should not be reset.
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Renderer-initiated nav to a different host shouldn't reset the state.
  rfh_tester->NavigateAndCommitRendererInitiated(true,
                                                 GURL("http://fooey.com/bar"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set up a subframe. Navigations in the subframe shouldn't reset the state.
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);
  subframe_tester->SimulateNavigationCommit(GURL("http://foo.com"));
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  subframe_tester->SimulateNavigationCommit(GURL("http://foobargoo.com"));
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set up a blocked state.
  UpdateExpectations(CANCEL);
  CanDownload();
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // The state should not be reset on a renderer-initiated load to either the
  // same host or a different host, in either the main frame or the subframe.
  rfh_tester->NavigateAndCommitRendererInitiated(
      true, GURL("http://fooeybar.com/bar"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  rfh_tester->NavigateAndCommitRendererInitiated(true,
                                                 GURL("http://foo.com/bar"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  rfh_tester =
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame());
  subframe = rfh_tester->AppendChild("subframe");
  subframe_tester = content::RenderFrameHostTester::For(subframe);
  subframe_tester->SimulateNavigationCommit(GURL("http://foo.com"));
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  subframe_tester->SimulateNavigationCommit(GURL("http://foobarfoo.com"));
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Browser-initiated navigation to a different host, which should reset the
  // state.
  NavigateAndCommit(GURL("http://foobar.com"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set up an allow all state.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  UpdateExpectations(ACCEPT);
  CanDownload();
  ExpectAndResetCounts(1, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // The state should not be reset on a pending renderer-initiated load to
  // the same host.
  rfh_tester =
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame());
  rfh_tester->NavigateAndCommitRendererInitiated(true,
                                                 GURL("http://foobar.com/bar"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // The state should not be reset for a subframe nav to the same host.
  rfh_tester =
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame());
  subframe = rfh_tester->AppendChild("subframe");
  subframe_tester = content::RenderFrameHostTester::For(subframe);
  subframe_tester->SimulateNavigationCommit(GURL("http://foobar.com/bar"));
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  subframe_tester->SimulateNavigationCommit(GURL("http://foobarfoo.com/"));
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // But a pending load to a different host in the main frame should reset the
  // state.
  rfh_tester->NavigateAndCommitRendererInitiated(true,
                                                 GURL("http://foo.com"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_ResetOnUserGesture) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a user gesture with scroll, which should be ignored.
  OnUserInteraction(blink::WebInputEvent::kGestureScrollBegin);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  // Do a user gesture with mouse click, which should reset back to allow one.
  OnUserInteraction(blink::WebInputEvent::kMouseDown);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a touch event, which should reset back to allow one.
  OnUserInteraction(blink::WebInputEvent::kTouchStart);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do one download, which should change to prompt before download.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Do a user gesture with keyboard down, which should reset back to allow one.
  OnUserInteraction(blink::WebInputEvent::kRawKeyDown);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask twice, which triggers calling the delegate. Don't allow the download
  // so that we end up with not allowed.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  UpdateExpectations(CANCEL);
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  ExpectAndResetCounts(0, 1, 1, __LINE__);

  // A user gesture now should NOT change the state.
  OnUserInteraction(blink::WebInputEvent::kMouseDown);
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
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // If the user refreshes the page without responding to the infobar, pretend
  // like the refresh is the initial load: they get 1 free download (probably
  // the same as the actual initial load), then an infobar.
  UpdateExpectations(WAIT);

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(0, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  Reload();
  LoadCompleted();
  base::RunLoop().RunUntilIdle();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  ExpectAndResetCounts(1, 0, 0, __LINE__);

  UpdateExpectations(CANCEL);
  CanDownload();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  ExpectAndResetCounts(0, 1, 1, __LINE__);

  Reload();
  LoadCompleted();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest, DownloadRequestLimiter_RawWebContents) {
  std::unique_ptr<WebContents> web_contents(CreateTestWebContents());

  GURL url("http://foo.com/bar");
  web_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

// DownloadRequestLimiter won't try to make a permission request or infobar
// if there is no PermissionRequestManager/InfoBarService, and we want to
// test that it will Cancel() instead of prompting.
#if defined(OS_ANDROID)
  UnsetInfobarDelegate();
#endif
  ExpectAndResetCounts(0, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  // You get one freebie.
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  OnUserInteractionFor(web_contents.get(), blink::WebInputEvent::kTouchStart);
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
  OnUserInteractionFor(web_contents.get(), blink::WebInputEvent::kRawKeyDown);
  EXPECT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
  CanDownloadFor(web_contents.get());
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  EXPECT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents.get()));
}

TEST_F(DownloadRequestLimiterTest,
       DownloadRequestLimiter_SetHostContentSetting) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();
  SetHostContentSetting(web_contents(), CONTENT_SETTING_ALLOW);

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  SetHostContentSetting(web_contents(), CONTENT_SETTING_BLOCK);

  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}

TEST_F(DownloadRequestLimiterTest,
       DownloadRequestLimiter_ContentSettingChanged) {
  NavigateAndCommit(GURL("http://foo.com/bar"));
  LoadCompleted();
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Simulate an accidental deny.
  UpdateExpectations(CANCEL);
  CanDownload();
  ExpectAndResetCounts(0, 1, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set the content setting to allow and send the notification. Ensure that the
  // limiter states update to match.
  UpdateContentSettings(web_contents(), CONTENT_SETTING_ALLOW);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ask to download, and assert that it succeeded and we are still in allow.
  CanDownload();
  ExpectAndResetCounts(1, 0, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Set the content setting to block and send the notification. Ensure that the
  // limiter states updates to match.
  UpdateContentSettings(web_contents(), CONTENT_SETTING_BLOCK);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Ensure downloads are blocked.
  CanDownload();
  ExpectAndResetCounts(0, 1, 0, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED,
            download_request_limiter_->GetDownloadStatus(web_contents()));

  // Reset to ask. Verify that the download counts have not changed on the
  // content settings change (ensuring there is no "free" download after
  // changing the content setting).
  UpdateContentSettings(web_contents(), CONTENT_SETTING_ASK);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
  UpdateExpectations(WAIT);
  CanDownload();
  ExpectAndResetCounts(0, 0, 1, __LINE__);
  ASSERT_EQ(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD,
            download_request_limiter_->GetDownloadStatus(web_contents()));
}
