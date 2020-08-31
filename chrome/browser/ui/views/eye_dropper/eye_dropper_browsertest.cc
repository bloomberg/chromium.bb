// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/display/display_switches.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/eye_dropper/eye_dropper_win.h"
#endif

class EyeDropperBrowserTest : public UiBrowserTest,
                              public ::testing::WithParamInterface<float> {
 public:
  EyeDropperBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::NumberToString(GetParam()));
  }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    content::RenderFrameHost* parent_frame =
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    eye_dropper_ = ShowEyeDropper(parent_frame, /*listener=*/nullptr);
  }

  bool VerifyUi() override {
#if defined(OS_WIN)
    if (!eye_dropper_)
      return false;

    views::Widget* widget =
        static_cast<EyeDropperWin*>(eye_dropper_.get())->GetWidget();
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});
    return VerifyPixelUi(widget, "EyeDropperBrowserTest", screenshot_name);
#else
    return true;
#endif
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

  void DismissUi() override { eye_dropper_.reset(); }

 private:
  std::unique_ptr<content::EyeDropper> eye_dropper_;
};

// Invokes the eye dropper.
IN_PROC_BROWSER_TEST_P(EyeDropperBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         EyeDropperBrowserTest,
                         testing::Values(1.0, 1.5, 2.0));
