// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/test/gtest_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/platform_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/base/test/user_interactive_test_case.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"  // nogncheck
#endif

namespace {

// An automatic action for WidgetCloser to post to the RunLoop.
// TODO(tapted): Explore asynchronous Widget::Close() and DialogClientView::
// {Accept,Cancel}Window() approaches to test other dialog lifetimes.
enum class DialogAction {
  INTERACTIVE,  // Run interactively.
  CLOSE_NOW,    // Call Widget::CloseNow().
};

// Helper to break out of the nested run loop that runs a test dialog.
class WidgetCloser : public views::WidgetObserver {
 public:
  WidgetCloser(views::Widget* widget, DialogAction action)
      : widget_(widget), weak_ptr_factory_(this) {
    widget->AddObserver(this);
    if (action == DialogAction::INTERACTIVE)
      return;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&WidgetCloser::CloseNow, weak_ptr_factory_.GetWeakPtr()));
  }

  // WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
    base::MessageLoop::current()->QuitNow();
  }

 private:
  void CloseNow() {
    if (widget_)
      widget_->CloseNow();
  }

  views::Widget* widget_;

  base::WeakPtrFactory<WidgetCloser> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WidgetCloser);
};

// Extracts the |name| argument for ShowDialog() from the current test case.
// E.g. for InvokeDialog_name (or DISABLED_InvokeDialog_name) returns "name".
std::string NameFromTestCase() {
  const std::string name = base::TestNameWithoutDisabledPrefix(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  std::string::size_type underscore = name.find('_');
  return underscore == std::string::npos ? std::string()
                                         : name.substr(underscore + 1);
}

}  // namespace

TestBrowserDialog::TestBrowserDialog() {}

void TestBrowserDialog::RunDialog() {
#if defined(OS_MACOSX)
  // The rest of this method assumes the child dialog is toolkit-views. So, for
  // Mac, it will only work if --secondary-ui-md is passed. Without this, a
  // Cocoa dialog will be created, which TestBrowserDialog doesn't support.
  // Force SecondaryUiMaterial() on Mac to get coverage on the bots. Leave it
  // optional elsewhere so that the non-MD dialog can be invoked to compare.
  ui::test::MaterialDesignControllerTestAPI md_test_api(
      ui::MaterialDesignController::GetMode());
  md_test_api.SetSecondaryUiMaterial(true);
#endif

  views::Widget::Widgets widgets_before =
      views::test::WidgetTest::GetAllWidgets();
#if defined(OS_CHROMEOS)
  // GetAllWidgets() uses AuraTestHelper to find the aura root window, but
  // that's not used on browser_tests, so ask ash.
  views::Widget::GetAllChildWidgets(ash::Shell::GetPrimaryRootWindow(),
                                    &widgets_before);
#endif  // OS_CHROMEOS

  ShowDialog(NameFromTestCase());
  views::Widget::Widgets widgets_after =
      views::test::WidgetTest::GetAllWidgets();
#if defined(OS_CHROMEOS)
  views::Widget::GetAllChildWidgets(ash::Shell::GetPrimaryRootWindow(),
                                    &widgets_after);
#endif  // OS_CHROMEOS

  auto added = base::STLSetDifference<std::vector<views::Widget*>>(
      widgets_after, widgets_before);

  // This can fail if no dialog was shown, if the dialog shown wasn't a toolkit-
  // views dialog, or if more than one child dialog was shown.
  ASSERT_EQ(1u, added.size());

  const DialogAction action = base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  internal::kInteractiveSwitch)
                                  ? DialogAction::INTERACTIVE
                                  : DialogAction::CLOSE_NOW;

  WidgetCloser closer(added[0], action);
  ::test::RunTestInteractively();
}
