// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/task.h"
#include "chrome/browser/ui/views/accessible_pane_view.h"
#include "views/view.h"

namespace chromeos {

class AccessibilityMenuButton;
class CapsLockMenuButton;
class ClockMenuButton;
class InputMethodMenuButton;
class MemoryMenuButton;
class NetworkMenuButton;
class PowerMenuButton;
class StatusAreaHost;

// This class is used to wrap the small informative widgets in the upper-right
// of the window title bar. It is used on ChromeOS only.
class StatusAreaView : public AccessiblePaneView {
 public:
  explicit StatusAreaView(StatusAreaHost* host);
  virtual ~StatusAreaView() {}

  virtual void Init();
  void MakeButtonsActive(bool active);
  void ButtonVisibilityChanged(views::View* button_view);

  // Takes focus and transfers it to the first (last if |reverse| is true).
  // After focus has traversed through all elements, clears focus and calls
  // |return_focus_cb(reverse)| from the message loop.
  void TakeFocus(bool reverse,
                 const base::Callback<void(bool)>& return_focus_cb);

  // Overridden from views::FocusChangeListener:
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now) OVERRIDE;

  // views::View* overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void ChildPreferredSizeChanged(View* child);

  AccessibilityMenuButton* accessibility_view() { return accessibility_view_; }
  CapsLockMenuButton* caps_lock_view() { return caps_lock_view_; }
  ClockMenuButton* clock_view() { return clock_view_; }
  InputMethodMenuButton* input_method_view() { return input_method_view_; }
  NetworkMenuButton* network_view() { return network_view_; }
  PowerMenuButton* power_view() { return power_view_; }

 private:
  StatusAreaHost* host_;

  AccessibilityMenuButton* accessibility_view_;
  CapsLockMenuButton* caps_lock_view_;
  ClockMenuButton* clock_view_;
  InputMethodMenuButton* input_method_view_;
  MemoryMenuButton* memory_view_;
  NetworkMenuButton* network_view_;
  PowerMenuButton* power_view_;

  // True if focus needs to be returned via |return_focus_cb_| when it wraps.
  bool need_return_focus_;
  base::Callback<void(bool)> return_focus_cb_;
  ScopedRunnableMethodFactory<StatusAreaView> task_factory_;

  // Clears focus and calls |return_focus_cb_|.
  void ReturnFocus(bool reverse);

  DISALLOW_COPY_AND_ASSIGN(StatusAreaView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_H_
