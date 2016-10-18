// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char* kOrigin1 = "https://google.com";
const char* kOrigin2 = "https://maps.google.com";
const char* kOrigin3 = "https://example.com";

}  // namespace

class FlashTemporaryPermissionTrackerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tracker_ = new FlashTemporaryPermissionTracker(profile());
  }

 protected:
  FlashTemporaryPermissionTracker* tracker() { return tracker_.get(); }

  content::RenderFrameHost* GetMainRFH(const char* origin) {
    content::RenderFrameHost* result = web_contents()->GetMainFrame();
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    content::RenderFrameHostTester::For(result)->SimulateNavigationCommit(
        GURL(origin));
    return result;
  }

  content::RenderFrameHost* AddChildRFH(content::RenderFrameHost* parent,
                                        const char* origin) {
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChild("");
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    content::RenderFrameHostTester::For(result)->SimulateNavigationCommit(
        GURL(origin));
    return result;
  }

 private:
  scoped_refptr<FlashTemporaryPermissionTracker> tracker_;
};

TEST_F(FlashTemporaryPermissionTrackerTest, Basic) {
  GetMainRFH(kOrigin1);

  // Flash shouldn't be enabled initially.
  EXPECT_FALSE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
  tracker()->FlashEnabledForWebContents(web_contents());

  // Flash should be enabled now.
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Refresh the page.
  Reload();
  // Flash should still be enabled after a single refresh.
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Refresh again.
  Reload();
  // Flash shouldn't be enabled anymore.
  EXPECT_FALSE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
}

TEST_F(FlashTemporaryPermissionTrackerTest, NavigateAway) {
  content::RenderFrameHost* rfh = GetMainRFH(kOrigin1);

  tracker()->FlashEnabledForWebContents(web_contents());
  Reload();
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Navigate to another origin. Flash shouldn't be enabled anymore.
  content::RenderFrameHostTester::For(rfh)->SimulateNavigationCommit(
      GURL(kOrigin2));
  EXPECT_FALSE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
}

TEST_F(FlashTemporaryPermissionTrackerTest, NavigateChildFrame) {
  content::RenderFrameHost* rfh = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child = AddChildRFH(rfh, kOrigin2);

  tracker()->FlashEnabledForWebContents(web_contents());
  Reload();
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Navigate the child frame. Flash should still be enabled after this.
  content::RenderFrameHostTester::For(child)->SimulateNavigationCommit(
      GURL(kOrigin3));
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
}
