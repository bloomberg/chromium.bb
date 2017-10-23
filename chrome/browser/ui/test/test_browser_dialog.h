// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_H_
#define CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_H_

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"

// TestBrowserDialog provides a way to register an InProcessBrowserTest testing
// harness with a framework that invokes Chrome browser dialogs in a consistent
// way. It optionally provides a way to invoke dialogs "interactively". This
// allows screenshots to be generated easily, with the same test data, to assist
// with UI review. It also provides a registry of dialogs so they can be
// systematically checked for subtle changes and regressions.
//
// To use TestBrowserDialog, a test harness should inherit from
// DialogBrowserTest rather than InProcessBrowserTest. If the dialog under test
// has only a single mode of operation, the only other requirement on the test
// harness is an override:
//
// class FooDialogTest : public DialogBrowserTest {
//  public:
//   ..
//   // DialogBrowserTest:
//   void ShowDialog(const std::string& name) override {
//     /* Show dialog attached to browser() and leave it open. */
//   }
//   ..
// };
//
// then in the foo_dialog_browsertest.cc, define any number of
//
// IN_PROC_BROWSER_TEST_F(FooDialogTest, InvokeDialog_name) {
//   RunDialog();
// }
//
// The string after "InvokeDialog_" (here, "name") is the argument given to
// ShowDialog(). In a regular test suite run, RunDialog() shows the dialog and
// immediately closes it (after ensuring it was actually created).
//
// To get a list of all available dialogs, run the `BrowserDialogTest.Invoke`
// test case without other arguments. I.e.
//
//   browser_tests --gtest_filter=BrowserDialogTest.Invoke
//
// Dialogs listed can be shown interactively using the --dialog argument. E.g.
//
//   browser_tests --gtest_filter=BrowserDialogTest.Invoke --interactive
//       --dialog=FooDialogTest.InvokeDialog_name
class TestBrowserDialog {
 protected:
  TestBrowserDialog();

  // Runs the dialog whose name corresponds to the current test case.
  void RunDialog();

  // Convenience method to force-enable features::kSecondaryUiMd for this test
  // on all platforms. This should be called in an override of SetUp().
  void UseMdOnly();

  // Show the dialog corresponding to |name| and leave it open.
  virtual void ShowDialog(const std::string& name) = 0;

  // Whether to always close asynchronously using Widget::Close(). This covers
  // codepaths relying on DialogDelegate::Close(), which isn't invoked by
  // Widget::CloseNow(). Dialogs should support both, since the OS can initiate
  // the destruction of dialogs, e.g., during logoff which bypass
  // Widget::CanClose() and DialogDelegate::Close().
  virtual bool AlwaysCloseAsynchronously();

 private:
  base::test::ScopedFeatureList maybe_enable_md_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserDialog);
};

// Helper to mix in a TestBrowserDialog to an existing test harness. |Base|
// must be a descendant of InProcessBrowserTest.
template <class Base>
class SupportsTestDialog : public Base, public TestBrowserDialog {
 protected:
  template <class... Args>
  explicit SupportsTestDialog(Args&&... args)
      : Base(std::forward<Args>(args)...) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SupportsTestDialog);
};

using DialogBrowserTest = SupportsTestDialog<InProcessBrowserTest>;

namespace internal {

// When present on the command line, runs the test in an interactive mode.
constexpr const char kInteractiveSwitch[] = "interactive";

}  // namespace internal

#endif  // CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_H_
