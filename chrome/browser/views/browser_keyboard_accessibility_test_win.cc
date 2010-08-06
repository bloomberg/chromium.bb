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

class BrowserKeyboardAccessibility : public InProcessBrowserTest,
                                     public ChromeViewsDelegate {
 public:
  BrowserKeyboardAccessibility()
      : is_waiting_(false),
        current_view_(NULL),
        current_event_type_(AccessibilityTypes::EVENT_FOCUS) {
    // Set ourselves as the currently active ViewsDelegate.
    ViewsDelegate::views_delegate = this;
  }

  ~BrowserKeyboardAccessibility() {}

  // Overridden from ChromeViewsDelegate.
  // Save the last notification sent by views::View::NotifyAccessibilityEvent.
  virtual void NotifyAccessibilityEvent(
      views::View* view, AccessibilityTypes::Event event_type) {
    current_view_  = view;
    current_event_type_ = event_type;

    // Are we within a message loop waiting for a particular event?
    // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
    LOG(ERROR) << "Got notification; is_waiting_ is "
               << (is_waiting_ ? "true" : "false");
    if (is_waiting_) {
      is_waiting_ = false;
      MessageLoop::current()->Quit();
    }
  }

  // Helper that performs tabbing until it cycles back to the original focus.
  void TabCyclerForwardAndBack(gfx::NativeWindow hwnd);
  void TabCycler(gfx::NativeWindow hwnd, bool forward_tab);

  views::View* current_view() const { return current_view_; }

  gfx::NativeWindow current_view_native_window() const {
    return current_view()->GetWidget()->GetNativeView();
  }

  AccessibilityTypes::Event current_event() const {
    return current_event_type_;
  }

  void set_waiting(bool value) { is_waiting_ = value; }

 private:
  // Are we waiting for an event?
  bool is_waiting_;

  // View of interest (i.e. for testing or one we are waiting to gain focus).
  views::View* current_view_;

  // Event type of interest.
  AccessibilityTypes::Event current_event_type_;
};

// This test is disabled, see bug 50864.
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       DISABLED_TabInAboutChromeDialog) {
  views::Window* about_chrome_window =
      BrowserView::GetBrowserViewForNativeWindow(
          browser()->window()->GetNativeHandle())->ShowAboutChromeDialog();

  TabCyclerForwardAndBack(about_chrome_window->GetNativeWindow());
}

// This test is disabled, see bug 50864.
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       DISABLED_TabInClearBrowsingDataDialog) {
  browser()->OpenClearBrowsingDataDialog();
  TabCyclerForwardAndBack(current_view_native_window());
}

// This test is disabled, see bug 50864.
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       DISABLED_TabInImportSettingsDialog) {
  browser()->OpenImportSettingsDialog();
  TabCyclerForwardAndBack(current_view_native_window());
}

// This test is disabled, see bug 50864.
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       DISABLED_TabInKeywordEditor) {
  browser()->OpenKeywordEditor();
  TabCyclerForwardAndBack(current_view_native_window());
}

// This test is disabled, see bug 50864.
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       DISABLED_TabInOptionsDialog) {
  browser()->OpenOptionsDialog();

  // Tab through each of the three tabs.
  for (int i = 0; i < 3; ++i) {
    // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
    LOG(ERROR) << "Iteration no. " << i;

    TabCyclerForwardAndBack(current_view_native_window());


    // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
    LOG(ERROR) << "Sending TAB key event...";
    ui_controls::SendKeyPressNotifyWhenDone(current_view_native_window(),
                                            base::VKEY_TAB,
                                            true, false, false, false,
                                            new MessageLoop::QuitTask());
    set_waiting(true);
    ui_test_utils::RunMessageLoop();
  }
}

// This test is disabled, see bug 50864.
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       DISABLED_TabInPasswordManager) {
  browser()->OpenPasswordManager();
  TabCyclerForwardAndBack(current_view_native_window());
}

// TODO(dtseng): http://www.crbug.com/50402
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       FAILS_TabInSyncMyBookmarksDialog) {
  browser()->OpenSyncMyBookmarksDialog();
  TabCyclerForwardAndBack(current_view_native_window());
}

// This test is disabled, see bug 50864.
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       DISABLED_TabInTaskManager) {
  browser()->OpenTaskManager();
  TabCyclerForwardAndBack(current_view_native_window());
}

// This test is disabled, see bug 50864.
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       DISABLED_TabInToolbar) {
  gfx::NativeWindow native_window = browser()->window()->GetNativeHandle();
  ui_controls::SendKeyPressNotifyWhenDone(native_window,
                                          base::VKEY_T,
                                          false, true, true, false,
                                          new MessageLoop::QuitTask());
  set_waiting(true);
  ui_test_utils::RunMessageLoop();
  TabCyclerForwardAndBack(current_view_native_window());
}

// This test is disabled, see bug 50864.
IN_PROC_BROWSER_TEST_F(BrowserKeyboardAccessibility,
                       DISABLED_TabInUpdateChromeDialog) {
  browser()->OpenUpdateChromeDialog();
  TabCyclerForwardAndBack(current_view_native_window());
}

void BrowserKeyboardAccessibility::TabCyclerForwardAndBack(
    gfx::NativeWindow hwnd) {
  TabCycler(hwnd, true);
  TabCycler(hwnd, false);
}

void BrowserKeyboardAccessibility::TabCycler(gfx::NativeWindow hwnd,
                                             bool forward_tab) {
  // Wait for a focus event on the provided native window.
  while (current_event() != AccessibilityTypes::EVENT_FOCUS ||
         current_view_native_window() != hwnd) {
    // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
    LOG(ERROR) << "Runnig message loop...";
    set_waiting(true);
    ui_test_utils::RunMessageLoop();
  }
  // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
  LOG(ERROR) << "After the loop.";

  views::View* first_focused_item = current_view();

  ASSERT_TRUE(first_focused_item != NULL);

  views::View* next_focused_item = first_focused_item;

  // Keep tabbing until we reach the originally focused view.
  do {
    // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
    LOG(ERROR) << "Sending TAB key event.";
    ui_controls::SendKeyPressNotifyWhenDone(hwnd, base::VKEY_TAB,
        false, !forward_tab, false, false, new MessageLoop::QuitTask());
    set_waiting(true);
    ui_test_utils::RunMessageLoop();
    next_focused_item = current_view();
  } while (first_focused_item != next_focused_item);
  // TODO(phajdan.jr): remove logging after fixing http://crbug.com/50663.
  LOG(ERROR) << "After second loop.";
}

}  // namespace
