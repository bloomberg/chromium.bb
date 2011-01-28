// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/status_area_view.h"

#include <algorithm>

#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/power_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/chromeos/status/window_switcher_button.h"
#include "gfx/canvas.h"

namespace chromeos {

// Number of pixels to separate each icon.
const int kSeparation = 1;

StatusAreaView::StatusAreaView(StatusAreaHost* host)
    : host_(host),
      clock_view_(NULL),
      input_method_view_(NULL),
      network_view_(NULL),
      power_view_(NULL),
      window_switcher_view_(NULL) {
}

void StatusAreaView::Init() {
  // Clock.
  clock_view_ = new ClockMenuButton(host_);
  AddChildView(clock_view_);

  // InputMethod.
  input_method_view_ = new InputMethodMenuButton(host_);
  AddChildView(input_method_view_);

  // Network.
  network_view_ = new NetworkMenuButton(host_);
  AddChildView(network_view_);

  // Power.
  power_view_ = new PowerMenuButton();
  AddChildView(power_view_);

  // Window Switcher.
  window_switcher_view_ = new WindowSwitcherButton(host_);
  AddChildView(window_switcher_view_);
}

gfx::Size StatusAreaView::GetPreferredSize() {
  int result_w = kSeparation;
  int result_h = 0;
  for (int i = 0; i < GetChildViewCount(); i++) {
    views::View* cur = GetChildViewAt(i);
    if (cur->IsVisible()) {
      gfx::Size cur_size = cur->GetPreferredSize();
      // Add each width.
      result_w += cur_size.width() + kSeparation;
      // Use max height.
      result_h = std::max(result_h, cur_size.height());
    }
  }
  return gfx::Size(result_w, result_h);
}

void StatusAreaView::Layout() {
  int cur_x = kSeparation;
  for (int i = 0; i < GetChildViewCount(); i++) {
    views::View* cur = GetChildViewAt(i);
    if (cur->IsVisible()) {
      gfx::Size cur_size = cur->GetPreferredSize();
      int cur_y = (height() - cur_size.height()) / 2;

      // Handle odd number of pixels.
      cur_y += (height() - cur_size.height()) % 2;

      // Put next in row horizontally, and center vertically.
      cur->SetBounds(cur_x, cur_y, cur_size.width(), cur_size.height());

      if (cur_size.width() > 0)
        cur_x += cur_size.width() + kSeparation;
    }
  }
}

void StatusAreaView::ChildPreferredSizeChanged(View* child) {
  // When something like the clock menu button's size changes, we need to
  // relayout. Also mark that this view's size has changed. This will let
  // BrowserView know to relayout, which will reset the bounds of this view.
  Layout();
  PreferredSizeChanged();
}

void StatusAreaView::MakeButtonsActive(bool active) {
  clock_view()->set_active(active);
  input_method_view()->set_active(active);
  network_view()->set_active(active);
  power_view()->set_active(active);
}

}  // namespace chromeos
