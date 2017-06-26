// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {

class FakeLocalSessionEventHandler : public LocalSessionEventHandler {
 public:
  void OnLocalTabModified(SyncedTabDelegate* modified_tab) override {
    was_notified_ = true;
  }

  void OnFaviconsChanged(const std::set<GURL>& page_urls,
                         const GURL& icon_url) override {}

  bool was_notified_since_last_call() {
    bool was_notified = was_notified_;
    was_notified_ = false;
    return was_notified;
  }

 private:
  bool was_notified_;
};

class SyncSessionsRouterTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  SyncSessionsRouterTabHelperTest() : ChromeRenderViewHostTestHarness() {}
  ~SyncSessionsRouterTabHelperTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    router_ =
        SyncSessionsWebContentsRouterFactory::GetInstance()->GetForProfile(
            profile());
    SyncSessionsRouterTabHelper::CreateForWebContents(web_contents(), router_);
    router_->StartRoutingTo(handler());

    TabContentsSyncedTabDelegate::CreateForWebContents(web_contents());
    NavigateAndCommit(GURL("about:blank"));
  }

  SyncSessionsWebContentsRouter* router() { return router_; }
  FakeLocalSessionEventHandler* handler() { return &handler_; }

 private:
  SyncSessionsWebContentsRouter* router_;
  FakeLocalSessionEventHandler handler_;
};

TEST_F(SyncSessionsRouterTabHelperTest, SubframeNavigationsIgnored) {
  SyncSessionsRouterTabHelper* helper =
      SyncSessionsRouterTabHelper::FromWebContents(web_contents());

  EXPECT_TRUE(handler()->was_notified_since_last_call());

  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  GURL child_url("http://foobar.com");

  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(child_url,
                                                                  child_rfh);
  helper->DidFinishNavigation(test_handle.get());
  EXPECT_FALSE(handler()->was_notified_since_last_call());

  helper->DidFinishLoad(child_rfh, GURL());
  EXPECT_FALSE(handler()->was_notified_since_last_call());
}

}  // namespace sync_sessions
