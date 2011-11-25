// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_delegate.h"
#include "views/controls/textfield/textfield.h"

namespace {

class TestConstrainedDialogContentsView
    : public views::View,
      public base::SupportsWeakPtr<TestConstrainedDialogContentsView> {
 public:
  TestConstrainedDialogContentsView()
      : text_field_(new views::Textfield) {
    SetLayoutManager(new views::FillLayout);
    AddChildView(text_field_);
  }

  views::View* GetInitiallyFocusedView() {
    return text_field_;
  }

 private:
  views::Textfield* text_field_;
  DISALLOW_COPY_AND_ASSIGN(TestConstrainedDialogContentsView);
};

class TestConstrainedDialog : public views::DialogDelegate {
 public:
  TestConstrainedDialog()
      : contents_((new TestConstrainedDialogContentsView())->AsWeakPtr()),
        done_(false) {
  }

  ~TestConstrainedDialog() {}

  virtual views::View* GetInitiallyFocusedView() OVERRIDE {
    return contents_ ? contents_->GetInitiallyFocusedView() : NULL;
  }

  virtual views::View* GetContentsView() OVERRIDE {
    return contents_.get();
  }

  virtual views::Widget* GetWidget() OVERRIDE {
    return contents_ ? contents_->GetWidget() : NULL;
  }

  virtual const views::Widget* GetWidget() const OVERRIDE {
    return contents_ ? contents_->GetWidget() : NULL;
  }

  virtual void DeleteDelegate() OVERRIDE {
    // Don't delete the delegate yet.  We need to keep it around for inspection
    // later.
    EXPECT_TRUE(done_);
  }

  virtual bool Accept() OVERRIDE {
    done_ = true;
    return true;
  }

  virtual bool Cancel() OVERRIDE {
    done_ = true;
    return true;
  }

  bool done() {
    return done_;
  }

 private:
  // contents_ will be freed when the View goes away.
  base::WeakPtr<TestConstrainedDialogContentsView> contents_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(TestConstrainedDialog);
};

} // namespace

class ConstrainedWindowViewTest : public InProcessBrowserTest {
 public:
  ConstrainedWindowViewTest() {
    set_show_window(true);
  }
};

// Tests the following:
//
// *) Initially focused view in a constrained dialog receives focus reliably.
//
// *) Constrained windows that are queued don't register themselves as
//    accelerator targets until they are displayed.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest, FocusTest) {
  TabContentsWrapper* tab_contents = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(tab_contents != NULL);
  ConstrainedWindowTabHelper* constrained_window_helper =
      tab_contents->constrained_window_tab_helper();
  ASSERT_TRUE(constrained_window_helper != NULL);

  // Create a constrained dialog.  It will attach itself to tab_contents.
  scoped_ptr<TestConstrainedDialog> test_dialog1(new TestConstrainedDialog);
  ConstrainedWindowViews* window1 =
      new ConstrainedWindowViews(tab_contents, test_dialog1.get());

  views::FocusManager* focus_manager = window1->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  // old_target should be the OK button for test_dialog1.
  ui::AcceleratorTarget* old_target =
      focus_manager->GetCurrentTargetForAccelerator(
          ui::Accelerator(ui::VKEY_RETURN, false, false, false));
  ASSERT_TRUE(old_target != NULL);
  // test_dialog1's text field should be focused.
  EXPECT_EQ(test_dialog1->GetInitiallyFocusedView(),
            focus_manager->GetFocusedView());

  // Now create a second constrained dialog.  This will also be attached to
  // tab_contents, but will remain hidden since the test_dialog1 is still
  // showing.
  scoped_ptr<TestConstrainedDialog> test_dialog2(new TestConstrainedDialog);
  ConstrainedWindowViews* window2 =
      new ConstrainedWindowViews(tab_contents, test_dialog2.get());
  // Should be the same focus_manager.
  ASSERT_EQ(focus_manager, window2->GetFocusManager());

  // new_target should be the same as old_target since test_dialog2 is still
  // hidden.
  ui::AcceleratorTarget* new_target =
      focus_manager->GetCurrentTargetForAccelerator(
          ui::Accelerator(ui::VKEY_RETURN, false, false, false));
  ASSERT_TRUE(new_target != NULL);
  EXPECT_EQ(old_target, new_target);

  // test_dialog1's text field should still be the view that has focus.
  EXPECT_EQ(test_dialog1->GetInitiallyFocusedView(),
            focus_manager->GetFocusedView());
  ASSERT_EQ(2u, constrained_window_helper->constrained_window_count());

  // Now send a VKEY_RETURN to the browser.  This should result in closing
  // test_dialog1.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(
      ui::Accelerator(ui::VKEY_RETURN, false, false, false)));
  ui_test_utils::RunAllPendingInMessageLoop();

  EXPECT_TRUE(test_dialog1->done());
  EXPECT_FALSE(test_dialog2->done());
  EXPECT_EQ(1u, constrained_window_helper->constrained_window_count());

  // test_dialog2 will be shown.  Focus should be on test_dialog2's text field.
  EXPECT_EQ(test_dialog2->GetInitiallyFocusedView(),
            focus_manager->GetFocusedView());

  // Send another VKEY_RETURN, closing test_dialog2
  EXPECT_TRUE(focus_manager->ProcessAccelerator(
      ui::Accelerator(ui::VKEY_RETURN, false, false, false)));
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(test_dialog2->done());
  EXPECT_EQ(0u, constrained_window_helper->constrained_window_count());
}
