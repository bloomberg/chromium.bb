// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/keyboard_codes.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/views/chrome_views_delegate.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "views/view.h"
#include "views/accessibility/view_accessibility.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace {

// FocusWaiter is used to wait for focus on a NativeWindow. It is ViewsDelegate
// to know when focus changes.
class FocusWaiter : public ChromeViewsDelegate {
 public:
  // Wait for focus to be received on the window. Returns true if successful.
  static bool WaitForFocus(gfx::NativeWindow window) {
    FocusWaiter waiter(window);
    ui_test_utils::RunMessageLoop();
    // FocusWaiter exits the message loop when done.
    return waiter.quit_message_loop_;
  }

 private:
  FocusWaiter(gfx::NativeWindow window)
      : window_(window),
        last_delegate_(ViewsDelegate::views_delegate),
        quit_message_loop_(false) {
    ViewsDelegate::views_delegate = this;
  }

  ~FocusWaiter() {
    DCHECK_EQ(this, ViewsDelegate::views_delegate);
    ViewsDelegate::views_delegate = last_delegate_;
  }

  // Overidden from ChromeViewsDelegate.
  virtual void NotifyAccessibilityEvent(views::View* view,
                                        AccessibilityTypes::Event event_type) {
    if (!quit_message_loop_ && view->GetWidget()->GetNativeView() == window_) {
      quit_message_loop_ = true;
      MessageLoop::current()->Quit();
    }

    // Notify the existing delegate so that it can maintain whatever state it
    // needs to.
    if (last_delegate_)
      last_delegate_->NotifyAccessibilityEvent(view, event_type);
  }

 private:
  // Window we're waiting for focus on.
  gfx::NativeWindow window_;

  // ViewsDelegate at the time we were created.
  ViewsDelegate* last_delegate_;

  // Have we quit the message loop yet?
  bool quit_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(FocusWaiter);
};

class BrowserKeyboardAccessibility : public InProcessBrowserTest,
                                     public ChromeViewsDelegate {
 public:
  BrowserKeyboardAccessibility()
      : current_view_(NULL),
        current_event_type_(AccessibilityTypes::EVENT_FOCUS) {
    // Set ourselves as the currently active ViewsDelegate.
    ViewsDelegate::views_delegate = this;
  }

  ~BrowserKeyboardAccessibility() {}

  // Overridden from ChromeViewsDelegate.
  // Save the last notification sent by views::View::NotifyAccessibilityEvent.
  virtual void NotifyAccessibilityEvent(
      views::View* view, AccessibilityTypes::Event event_type) {
    current_view_ = view;
    current_event_type_ = event_type;
  }

  // Helper that performs tabbing until it cycles back to the original focus.
  void TabCyclerForwardAndBack();
  void TabCycler(bool forward_tab);

  views::View* current_view() const { return current_view_; }

  gfx::NativeWindow current_view_native_window() const {
    return current_view()->GetWidget()->GetNativeView();
  }

  AccessibilityTypes::Event current_event() const {
    return current_event_type_;
  }

 private:
  // View of interest (i.e. for testing or one we are waiting to gain focus).
  views::View* current_view_;

  // Event type of interest.
  AccessibilityTypes::Event current_event_type_;
};

IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility, TabInAboutChromeDialog) {
  views::Window* about_chrome_window =
      BrowserView::GetBrowserViewForNativeWindow(
          browser()->window()->GetNativeHandle())->ShowAboutChromeDialog();

  if (current_view_native_window() != about_chrome_window->GetNativeWindow()) {
    // Wait for 'about' to get focus.
    ASSERT_TRUE(
        FocusWaiter::WaitForFocus(about_chrome_window->GetNativeWindow()));
  }
  TabCyclerForwardAndBack();
}

IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       TabInClearBrowsingDataDialog) {
  browser()->OpenClearBrowsingDataDialog();
  TabCyclerForwardAndBack();
}

IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       TabInImportSettingsDialog) {
  browser()->OpenImportSettingsDialog();
  TabCyclerForwardAndBack();
}

IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility, TabInKeywordEditor) {
  browser()->OpenKeywordEditor();
  TabCyclerForwardAndBack();
}

IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility, TabInOptionsDialog) {
  browser()->OpenOptionsDialog();

  // Tab through each of the three tabs.
  for (int i = 0; i < 3 && !HasFatalFailure(); ++i) {
    // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
    LOG(ERROR) << "Iteration no. " << i;

    TabCyclerForwardAndBack();

    // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
    LOG(ERROR) << "Sending TAB key event...";
    ui_controls::SendKeyPressNotifyWhenDone(current_view_native_window(),
                                            base::VKEY_TAB,
                                            true, false, false, false,
                                            new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }
}

IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility, TabInPasswordManager) {
  browser()->OpenPasswordManager();
  TabCyclerForwardAndBack();
}

// TODO(dtseng): http://www.crbug.com/50402
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       FAILS_TabInSyncMyBookmarksDialog) {
  browser()->OpenSyncMyBookmarksDialog();
  TabCyclerForwardAndBack();
}

IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility, TabInTaskManager) {
  browser()->OpenTaskManager();
  TabCyclerForwardAndBack();
}

IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility, TabInToolbar) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeHandle();
  ui_controls::SendKeyPressNotifyWhenDone(native_window,
                                          base::VKEY_T,
                                          false, true, true, false,
                                          new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();

  // TODO: this needs to assert the toolbar got focus.

  TabCyclerForwardAndBack();
}

IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility, TabInUpdateChromeDialog) {
  browser()->OpenUpdateChromeDialog();
  TabCyclerForwardAndBack();
}

void BrowserKeyboardAccessibility::TabCyclerForwardAndBack() {
  if (HasFatalFailure())
    return;

  TabCycler(true);

  if (HasFatalFailure())
    return;

  TabCycler(false);
}

void BrowserKeyboardAccessibility::TabCycler(bool forward_tab) {
  gfx::NativeWindow hwnd = current_view_native_window();
  ASSERT_TRUE(hwnd);

  views::View* first_focused_item = current_view();

  ASSERT_TRUE(first_focused_item != NULL);

  // Keep tabbing until we reach the originally focused view.
  do {
    // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
    LOG(ERROR) << "Sending TAB key event.";
    ui_controls::SendKeyPressNotifyWhenDone(hwnd, base::VKEY_TAB,
        false, !forward_tab, false, false, new MessageLoop::QuitTask());

    current_view_ = NULL;

    ui_test_utils::RunMessageLoop();
  } while (first_focused_item != current_view() && !HasFatalFailure());
  // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
  LOG(ERROR) << "After second loop.";
}

}  // namespace
