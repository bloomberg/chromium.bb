// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/activation_tracker_win.h"

#include "base/time/time.h"

namespace {

static const wchar_t kTrayClassName[] = L"Shell_TrayWnd";

}  // namespace

ActivationTrackerWin::ActivationTrackerWin(
    app_list::AppListView* view,
    const base::Closure& on_should_dismiss)
    : view_(view),
      on_should_dismiss_(on_should_dismiss),
      regain_next_lost_focus_(false),
      preserving_focus_for_taskbar_menu_(false) {
  view_->AddObserver(this);
}

ActivationTrackerWin::~ActivationTrackerWin() {
  view_->RemoveObserver(this);
  timer_.Stop();
}

void ActivationTrackerWin::OnActivationChanged(
    views::Widget* widget, bool active) {
  const int kFocusCheckIntervalMS = 250;
  if (active) {
    timer_.Stop();
    return;
  }

  preserving_focus_for_taskbar_menu_ = false;
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kFocusCheckIntervalMS), this,
               &ActivationTrackerWin::CheckTaskbarOrViewHasFocus);
}

void ActivationTrackerWin::OnViewHidden() {
  timer_.Stop();
}

void ActivationTrackerWin::CheckTaskbarOrViewHasFocus() {
  // Remember if the taskbar had focus without the right mouse button being
  // down.
  bool was_preserving_focus = preserving_focus_for_taskbar_menu_;
  preserving_focus_for_taskbar_menu_ = false;

  // First get the taskbar and jump lists windows (the jump list is the
  // context menu which the taskbar uses).
  HWND jump_list_hwnd = FindWindow(L"DV2ControlHost", NULL);
  HWND taskbar_hwnd = FindWindow(kTrayClassName, NULL);

  // This code is designed to hide the app launcher when it loses focus,
  // except for the cases necessary to allow the launcher to be pinned or
  // closed via the taskbar context menu.
  // First work out if the left or right button is currently down.
  int swapped = GetSystemMetrics(SM_SWAPBUTTON);
  int left_button = swapped ? VK_RBUTTON : VK_LBUTTON;
  bool left_button_down = GetAsyncKeyState(left_button) < 0;
  int right_button = swapped ? VK_LBUTTON : VK_RBUTTON;
  bool right_button_down = GetAsyncKeyState(right_button) < 0;

  // Now get the window that currently has focus.
  HWND focused_hwnd = GetForegroundWindow();
  if (!focused_hwnd) {
    // Sometimes the focused window is NULL. This can happen when the focus is
    // changing due to a mouse button press. If the button is still being
    // pressed the launcher should not be hidden.
    if (right_button_down || left_button_down)
      return;

    // If the focused window is NULL, and the mouse button is not being
    // pressed, then the launcher no longer has focus.
    on_should_dismiss_.Run();
    return;
  }

  while (focused_hwnd) {
    // If the focused window is the right click menu (called a jump list) or
    // the app list, don't hide the launcher.
    if (focused_hwnd == jump_list_hwnd ||
        focused_hwnd == view_->GetHWND()) {
      return;
    }

    if (focused_hwnd == taskbar_hwnd) {
      // If the focused window is the taskbar, and the right button is down,
      // don't hide the launcher as the user might be bringing up the menu.
      if (right_button_down)
        return;

      // There is a short period between the right mouse button being down
      // and the menu gaining focus, where the taskbar has focus and no button
      // is down. If the taskbar is observed in this state once the launcher
      // is not dismissed. If it happens twice in a row it is dismissed.
      if (!was_preserving_focus) {
        preserving_focus_for_taskbar_menu_ = true;
        return;
      }

      break;
    }
    focused_hwnd = GetParent(focused_hwnd);
  }

  if (regain_next_lost_focus_) {
    regain_next_lost_focus_ = false;
    view_->GetWidget()->Activate();
    return;
  }

  // If we get here, the focused window is not the taskbar, it's context menu,
  // or the app list.
  on_should_dismiss_.Run();
}
