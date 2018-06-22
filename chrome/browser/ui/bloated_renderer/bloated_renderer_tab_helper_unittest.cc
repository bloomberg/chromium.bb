// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bloated_renderer/bloated_renderer_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/common/page_importance_signals.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class BloatedRendererTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    BloatedRendererTabHelper::CreateForWebContents(web_contents());
    tab_helper_ = BloatedRendererTabHelper::FromWebContents(web_contents());
    content::WebContentsTester::For(web_contents())
        ->SetLastCommittedURL(GURL("https://test.test"));
  }
  BloatedRendererTabHelper* tab_helper_;
};

TEST_F(BloatedRendererTabHelperTest, DetectReload) {
  EXPECT_FALSE(tab_helper_->reloading_bloated_renderer_);
  tab_helper_->reloading_bloated_renderer_ = true;
  tab_helper_->DidFinishNavigation(nullptr);
  EXPECT_FALSE(tab_helper_->reloading_bloated_renderer_);
}

TEST_F(BloatedRendererTabHelperTest, CanReloadBloatedTab) {
  EXPECT_TRUE(tab_helper_->CanReloadBloatedTab());
}

TEST_F(BloatedRendererTabHelperTest, CannotReloadBloatedTabCrashed) {
  web_contents()->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);

  EXPECT_FALSE(tab_helper_->CanReloadBloatedTab());
}

TEST_F(BloatedRendererTabHelperTest, CannotReloadBloatedTabInvalidURL) {
  content::WebContentsTester::For(web_contents())
      ->SetLastCommittedURL(GURL("invalid :)"));

  EXPECT_FALSE(tab_helper_->CanReloadBloatedTab());
}

TEST_F(BloatedRendererTabHelperTest,
       CannotReloadBloatedTabPendingUserInteraction) {
  content::PageImportanceSignals signals;
  signals.had_form_interaction = true;
  content::WebContentsTester::For(web_contents())
      ->SetPageImportanceSignals(signals);
  EXPECT_FALSE(tab_helper_->CanReloadBloatedTab());
}
