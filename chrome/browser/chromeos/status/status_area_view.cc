// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/status_area_view.h"

#include <algorithm>

#include "base/command_line.h"
#include "chrome/browser/chromeos/status/accessibility_menu_button.h"
#include "chrome/browser/chromeos/status/caps_lock_menu_button.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/input_method_menu_button.h"
#include "chrome/browser/chromeos/status/memory_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/power_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/common/chrome_switches.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/border.h"
#include "views/controls/image_view.h"

namespace chromeos {

// Number of pixels to separate each icon.
#if defined(TOUCH_UI)
const int kSeparation = 25;
#else
const int kSeparation = 5;
#endif

StatusAreaView::StatusAreaView(StatusAreaHost* host)
    : host_(host),
      accessibility_view_(NULL),
      caps_lock_view_(NULL),
      clock_view_(NULL),
      input_method_view_(NULL),
      memory_view_(NULL),
      network_view_(NULL),
      power_view_(NULL) {
}

void StatusAreaView::Init() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kMemoryWidget)) {
    memory_view_ = new MemoryMenuButton(host_);
    AddChildView(memory_view_);
  }

  accessibility_view_ = new AccessibilityMenuButton(host_);
  accessibility_view_->set_border(views::Border::CreateEmptyBorder(0, 1, 0, 0));
  AddChildView(accessibility_view_);

  caps_lock_view_ = new CapsLockMenuButton(host_);
  caps_lock_view_->set_border(views::Border::CreateEmptyBorder(0, 1, 0, 0));
  AddChildView(caps_lock_view_);

  clock_view_ = new ClockMenuButton(host_);
  clock_view_->set_border(views::Border::CreateEmptyBorder(0, 1, 0, 0));
  AddChildView(clock_view_);

  input_method_view_ = new InputMethodMenuButton(host_);
  AddChildView(input_method_view_);

  network_view_ = new NetworkMenuButton(host_);
  AddChildView(network_view_);

  power_view_ = new PowerMenuButton(host_);
  AddChildView(power_view_);
}

gfx::Size StatusAreaView::GetPreferredSize() {
  int result_w = 0;
  int result_h = 0;

  for (int i = 0; i < child_count(); i++) {
    views::View* cur = child_at(i);
    gfx::Size cur_size = cur->GetPreferredSize();
    if (cur->IsVisible() && !cur_size.IsEmpty()) {
      if (result_w == 0)
        result_w = kSeparation;

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
  for (int i = 0; i < child_count(); i++) {
    views::View* cur = child_at(i);
    gfx::Size cur_size = cur->GetPreferredSize();
    if (cur->IsVisible() && !cur_size.IsEmpty()) {
      int cur_y = (height() - cur_size.height()) / 2;

      // Handle odd number of pixels.
      cur_y += (height() - cur_size.height()) % 2;

      // Put next in row horizontally, and center vertically.
      cur->SetBounds(cur_x, cur_y, cur_size.width(), cur_size.height());
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
  if (memory_view_)
    memory_view_->set_active(active);
  accessibility_view()->set_active(active);
  caps_lock_view()->set_active(active);
  clock_view()->set_active(active);
  input_method_view()->set_active(active);
  network_view()->set_active(active);
  power_view()->set_active(active);
}

void StatusAreaView::ButtonVisibilityChanged(views::View* button_view) {
  Layout();
  PreferredSizeChanged();
}

}  // namespace chromeos
