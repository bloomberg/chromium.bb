// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/platform_util.h"
#include "ui/base/test/user_interactive_test_case.h"
#include "ui/base/ui_base_features.h"
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
  CLOSE,        // Call Widget::Close().
};

// Helper to break out of the nested run loop that runs a test dialog.
class WidgetCloser : public views::WidgetObserver {
 public:
  WidgetCloser(views::Widget* widget, DialogAction action)
      : action_(action), widget_(widget), weak_ptr_factory_(this) {
    widget->AddObserver(this);
    if (action == DialogAction::INTERACTIVE)
      return;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WidgetCloser::CloseAction,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  // WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
    base::RunLoop::QuitCurrentDeprecated();
  }

 private:
  void CloseAction() {
    if (!widget_)
      return;

    switch (action_) {
      case DialogAction::CLOSE_NOW:
        widget_->CloseNow();
        break;
      case DialogAction::CLOSE:
        widget_->Close();
        break;
      case DialogAction::INTERACTIVE:
        NOTREACHED();
        break;
    }
  }

  const DialogAction action_;
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
  // Mac, it will only work when MD for secondary UI is enabled. Without this, a
  // Cocoa dialog will be created, which TestBrowserDialog doesn't support.
  // Force kSecondaryUiMd on Mac to get coverage on the bots. Leave it optional
  // elsewhere so that the non-MD dialog can be invoked to compare. Note that
  // since SetUp() has already been called, some parts of the toolkit may
  // already be initialized without MD - this is just to ensure Cocoa dialogs
  // are not selected.
  base::test::ScopedFeatureList enable_views_on_mac_always;
  enable_views_on_mac_always.InitAndEnableFeature(features::kSecondaryUiMd);
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

  if (added.size() > 1) {
    // Some tests create a standalone window to anchor a dialog. In those cases,
    // ignore added Widgets that are not dialogs.
    base::EraseIf(added, [](views::Widget* widget) {
      return !widget->widget_delegate()->AsDialogDelegate();
    });
  }

  // This can fail if no dialog was shown, if the dialog shown wasn't a toolkit-
  // views dialog, or if more than one child dialog was shown.
  ASSERT_EQ(1u, added.size());

  DialogAction action = DialogAction::CLOSE_NOW;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          internal::kInteractiveSwitch)) {
    action = DialogAction::INTERACTIVE;
  } else if (AlwaysCloseAsynchronously()) {
    // TODO(tapted): Iterate over close methods when non-interactive for greater
    // test coverage.
    action = DialogAction::CLOSE;
  }

  WidgetCloser closer(added[0], action);
  ::test::RunTestInteractively();
}

void TestBrowserDialog::UseMdOnly() {
  maybe_enable_md_.InitAndEnableFeature(features::kSecondaryUiMd);
}

bool TestBrowserDialog::AlwaysCloseAsynchronously() {
  return false;
}
