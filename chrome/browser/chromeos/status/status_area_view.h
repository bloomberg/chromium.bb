// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/views/accessible_pane_view.h"
#include "views/view.h"

namespace chromeos {

class ClockMenuButton;
class InputMethodMenuButton;
class NetworkMenuButton;
class PowerMenuButton;
class StatusAreaHost;
class WindowSwitcherButton;

// This class is used to wrap the small informative widgets in the upper-right
// of the window title bar. It is used on ChromeOS only.
class StatusAreaView : public AccessiblePaneView {
 public:
  explicit StatusAreaView(StatusAreaHost* host);
  virtual ~StatusAreaView() {}

  virtual void Init();
  void MakeButtonsActive(bool active);

  // views::View* overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void ChildPreferredSizeChanged(View* child);

  ClockMenuButton* clock_view() { return clock_view_; }
  InputMethodMenuButton* input_method_view() { return input_method_view_; }
  NetworkMenuButton* network_view() { return network_view_; }
  PowerMenuButton* power_view() { return power_view_; }
  WindowSwitcherButton* window_switcher_view() {
    return window_switcher_view_;
  }

 private:
  StatusAreaHost* host_;

  ClockMenuButton* clock_view_;
  InputMethodMenuButton* input_method_view_;
  NetworkMenuButton* network_view_;
  PowerMenuButton* power_view_;
  WindowSwitcherButton* window_switcher_view_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_VIEW_H_
