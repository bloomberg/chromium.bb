// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class AutomationApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableAutomationAPI);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }
};

// TODO(dtseng): See crbug.com/360297.
#if defined(OS_MACOSX)
#define MAYBE_SanityCheck DISABLED_SanityCheck
#else
#define MAYBE_SanityCheck SanityCheck
#endif  // defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(AutomationApiTest, MAYBE_SanityCheck) {
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::RenderWidgetHost* rwh =
      tab->GetRenderWidgetHostView()->GetRenderWidgetHost();
  ASSERT_NE((content::RenderWidgetHost*)NULL, rwh)
      << "Couldn't get RenderWidgetHost";
  ASSERT_FALSE(rwh->IsFullAccessibilityModeForTesting());
  ASSERT_FALSE(rwh->IsTreeOnlyAccessibilityModeForTesting());

  ASSERT_TRUE(RunComponentExtensionTest("automation/sanity_check")) << message_;

  rwh = tab->GetRenderWidgetHostView()->GetRenderWidgetHost();
  ASSERT_NE((content::RenderWidgetHost*)NULL, rwh)
      << "Couldn't get RenderWidgetHost";
  ASSERT_FALSE(rwh->IsFullAccessibilityModeForTesting());
  ASSERT_TRUE(rwh->IsTreeOnlyAccessibilityModeForTesting());
}

}  // namespace extensions
