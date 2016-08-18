// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/web_contents_modal_dialog_manager_views_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

using views::Widget;

namespace {

class WebContentsModalDialogManagerViewsMacTest : public InProcessBrowserTest,
                                                  public views::WidgetObserver {
 public:
  WebContentsModalDialogManagerViewsMacTest() {}

  Widget* ShowViewsDialogOn(int web_contents_index, bool activate) {
    if (activate)
      browser()->tab_strip_model()->ActivateTabAt(web_contents_index, true);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(web_contents_index);

    DCHECK(web_contents);

    // Show a dialog as a constrained window modal to the current tab.
    Widget* widget = constrained_window::ShowWebModalDialogViews(new TestDialog,
                                                                 web_contents);
    widget->AddObserver(this);
    return widget;
  }

  void WaitForClose() {
    last_destroyed_ = nullptr;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
    run_loop_ = nullptr;
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    destroy_count_++;
    widget->RemoveObserver(this);
    last_destroyed_ = widget;
    if (run_loop_)
      run_loop_->Quit();
  }

  class TestDialog : public views::DialogDelegateView {
   public:
    TestDialog() {}
    // WidgetDelegate:
    ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

   private:
    DISALLOW_COPY_AND_ASSIGN(TestDialog);
  };

 protected:
  int destroy_count_ = 0;
  Widget* last_destroyed_ = nullptr;
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebContentsModalDialogManagerViewsMacTest);
};

}  // namespace

// Basic test for showing and dismissing the dialog in a single tab.
IN_PROC_BROWSER_TEST_F(WebContentsModalDialogManagerViewsMacTest, Basic) {
  // Cocoa browsers start with one child window (a StatusBubbleWindow).
  NSArray* children = [browser()->window()->GetNativeWindow() childWindows];
  EXPECT_EQ(1u, [children count]);

  Widget* dialog = ShowViewsDialogOn(0, true);
  EXPECT_TRUE(dialog->IsVisible());

  // The browser window will get two child windows: a transparent overlay, and
  // the views::Widget dialog. Ensure the dialog is on top.
  children = [browser()->window()->GetNativeWindow() childWindows];
  EXPECT_EQ(3u, [children count]);
  EXPECT_EQ(dialog->GetNativeWindow(), [children objectAtIndex:2]);

  // Toolkit-views dialogs use GetWidget()->Close() to dismiss themselves.
  dialog->Close();
  EXPECT_EQ(0, destroy_count_);
  WaitForClose();
  EXPECT_EQ(dialog, last_destroyed_);
  EXPECT_EQ(1, destroy_count_);
}

// Test showing dialogs in two tabs, switch tabs, then close the background tab,
// then close the browser window.
IN_PROC_BROWSER_TEST_F(WebContentsModalDialogManagerViewsMacTest,
                       TwoDialogsThenCloseTabs) {
  // Add a second tab, then show a dialog on each while the tab is active.
  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
  Widget* dialog1 = ShowViewsDialogOn(1, true);
  Widget* dialog0 = ShowViewsDialogOn(0, true);
  // On Mac, both dialogs report themselves as visible to avoid Spaces
  // transitions when switching tabs.
  EXPECT_TRUE(dialog0->IsVisible());
  EXPECT_TRUE(dialog1->IsVisible());

  // But the inactive, second dialog will be fully transparent.
  EXPECT_EQ(0.0, [dialog1->GetNativeWindow() alphaValue]);
  EXPECT_EQ(1.0, [dialog0->GetNativeWindow() alphaValue]);

  // Activate the second tab.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_EQ(1.0, [dialog1->GetNativeWindow() alphaValue]);
  EXPECT_EQ(0.0, [dialog0->GetNativeWindow() alphaValue]);

  // Close the background tab.
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabStripModel::CLOSE_USER_GESTURE);
  // Close is asynchronous, so nothing happens until a run loop is spun up.
  EXPECT_EQ(0, destroy_count_);
  WaitForClose();
  EXPECT_EQ(dialog0, last_destroyed_);
  EXPECT_EQ(1, destroy_count_);

  // Close the window. But first close all tabs. Otherwise before-unload
  // handlers have an opportunity to run, which is asynchronous.
  browser()->tab_strip_model()->CloseAllTabs();
  // Since the parent NSWindow goes away, close happens immediately.
  browser()->window()->Close();
  EXPECT_EQ(dialog1, last_destroyed_);
  EXPECT_EQ(2, destroy_count_);
}

// Test showing in a dialog in an inactive tab. For these, the WebContentsView
// will be detached from the view hierarchy.
IN_PROC_BROWSER_TEST_F(WebContentsModalDialogManagerViewsMacTest,
                       DialogInBackgroundTab) {
  // Add a second tab. It should start active.
  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  // Showing shouldn't change the active tab, and the dialog shouldn't be shown
  // at all yet.
  Widget* dialog0 = ShowViewsDialogOn(0, false);
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));
  EXPECT_FALSE(dialog0->IsVisible());

  // But switching to the tab should show (and animate) it.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabSelected(0));
  EXPECT_TRUE(dialog0->IsVisible());

  // Leave the dialog open for a change to ensure shutdown is clean.
}
