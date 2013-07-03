// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_controller.h"

#include "base/files/file_path.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

const char kSimple[] = "simple.html";

GURL GetURL(const std::string& filename) {
  return ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("find_in_page"),
                                   base::FilePath().AppendASCII(filename));
}

class FindBarControllerTest : public InProcessBrowserTest {
 public:
  FindBarControllerTest() {
    chrome::DisableFindBarAnimationsDuringTesting(true);
  }
};

// Make sure Find box grabs the Esc accelerator and restores it again.
IN_PROC_BROWSER_TEST_F(FindBarControllerTest, AcceleratorRestoring) {
  // First we navigate to any page.
  ui_test_utils::NavigateToURL(browser(), GetURL(kSimple));

  gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::FocusManager* focus_manager = widget->GetFocusManager();

  // See where Escape is registered.
  ui::Accelerator escape(ui::VKEY_ESCAPE, ui::EF_NONE);
  ui::AcceleratorTarget* old_target =
      focus_manager->GetCurrentTargetForAccelerator(escape);
  EXPECT_TRUE(old_target != NULL);

  chrome::ShowFindBar(browser());

  // Our Find bar should be the new target.
  ui::AcceleratorTarget* new_target =
      focus_manager->GetCurrentTargetForAccelerator(escape);

  EXPECT_TRUE(new_target != NULL);
  EXPECT_NE(new_target, old_target);

  // Close the Find box.
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);

  // The accelerator for Escape should be back to what it was before.
  EXPECT_EQ(old_target,
            focus_manager->GetCurrentTargetForAccelerator(escape));

  // Show find bar again with animation on, and the target should be on
  // find bar.
  chrome::DisableFindBarAnimationsDuringTesting(false);
  chrome::ShowFindBar(browser());
  EXPECT_EQ(new_target,
            focus_manager->GetCurrentTargetForAccelerator(escape));
}

}  // namespace
