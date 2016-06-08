// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

using subresource_filter::testing::ScopedSubresourceFilterFeatureToggle;

class SubresourceFilterBrowserTest : public InProcessBrowserTest {
 public:
  SubresourceFilterBrowserTest() {}
  ~SubresourceFilterBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    scoped_feature_toggle_.reset(new ScopedSubresourceFilterFeatureToggle(
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* FindFrameByName(const std::string& name) {
    for (content::RenderFrameHost* frame : web_contents()->GetAllFrames()) {
      if (frame->GetFrameName() == name)
        return frame;
    }
    return nullptr;
  }

  void ExpectScriptResourceWasNotLoaded(content::RenderFrameHost* rfh) {
    bool script_resource_was_not_loaded;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        rfh, "domAutomationController.send(!document.scriptExecuted)",
        &script_resource_was_not_loaded));
    EXPECT_TRUE(script_resource_was_not_loaded);
  }

 private:
  std::unique_ptr<ScopedSubresourceFilterFeatureToggle> scoped_feature_toggle_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, MainFrameActivation) {
  const GURL url(ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("subresource_filter"),
      base::FilePath().AppendASCII("frame_with_included_script.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  ExpectScriptResourceWasNotLoaded(web_contents()->GetMainFrame());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, SubFrameActivation) {
  const GURL url(ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("subresource_filter"),
      base::FilePath().AppendASCII("frame_set.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  const char* kSubframeNames[] = {"one", "two"};
  for (const char* subframe_name : kSubframeNames) {
    content::RenderFrameHost* frame = FindFrameByName(subframe_name);
    ASSERT_TRUE(frame);
    ExpectScriptResourceWasNotLoaded(frame);
  }
}

}  // namespace subresource_filter
