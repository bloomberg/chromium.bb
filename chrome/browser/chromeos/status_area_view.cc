// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status_area_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "chrome/browser/chromeos/clock_menu_button.h"
#include "chrome/browser/chromeos/language_menu_button.h"
#include "chrome/browser/chromeos/network_menu_button.h"
#include "chrome/browser/chromeos/power_menu_button.h"
#include "chrome/browser/chromeos/status_area_host.h"

namespace chromeos {

// Number of pixels to pad on the left border.
const int kLeftBorder = 1;
// Number of pixels to separate the clock from the next item on the right.
const int kClockSeparation = 4;
// Number of pixels to separate the language selector from the next item
// on the right.
const int kLanguageSeparation = 4;

// BrowserWindowGtk tiles its image with this offset
const int kCustomFrameBackgroundVerticalOffset = 15;

// Default to opening new tabs on the left.
StatusAreaView::OpenTabsMode StatusAreaView::open_tabs_mode_ =
    StatusAreaView::OPEN_TABS_ON_LEFT;

StatusAreaView::StatusAreaView(StatusAreaHost* host)
    : host_(host),
      clock_view_(NULL),
      language_view_(NULL),
      network_view_(NULL),
      power_view_(NULL) {
}

void StatusAreaView::Init() {
  // Language.
  language_view_ = new LanguageMenuButton(host_);
  AddChildView(language_view_);

  // Clock.
  clock_view_ = new ClockMenuButton(host_);
  AddChildView(clock_view_);

  // Power.
  power_view_ = new PowerMenuButton();
  AddChildView(power_view_);

  // Network.
  network_view_ = new NetworkMenuButton(
      host_->GetNativeWindow());
  AddChildView(network_view_);
}

void StatusAreaView::Update() {
  for (int i = 0; i < GetChildViewCount(); ++i) {
    views::View* cur = GetChildViewAt(i);
    cur->SetVisible(host_->IsButtonVisible(cur));
  }
}

gfx::Size StatusAreaView::GetPreferredSize() {
  // Start with padding.
  int result_w = kLeftBorder + kClockSeparation + kLanguageSeparation;
  int result_h = 0;
  for (int i = 0; i < GetChildViewCount(); i++) {
    views::View* cur = GetChildViewAt(i);
    if (cur->IsVisible()) {
      gfx::Size cur_size = cur->GetPreferredSize();
      // Add each width.
      result_w += cur_size.width();
      // Use max height.
      result_h = std::max(result_h, cur_size.height());
    }
  }
  return gfx::Size(result_w, result_h);
}

void StatusAreaView::Layout() {
  int cur_x = kLeftBorder;
  for (int i = 0; i < GetChildViewCount(); i++) {
    views::View* cur = GetChildViewAt(i);
    if (cur->IsVisible()) {
      gfx::Size cur_size = cur->GetPreferredSize();
      int cur_y = (height() - cur_size.height()) / 2;

      // Handle odd number of pixels.
      cur_y += (height() - cur_size.height()) % 2;

      // Put next in row horizontally, and center vertically.
      cur->SetBounds(cur_x, cur_y, cur_size.width(), cur_size.height());

      cur_x += cur_size.width();

      // Buttons have built in padding, but clock and language status don't.
      if (cur == clock_view_)
        cur_x += kClockSeparation;
      else if (cur == language_view_)
        cur_x += kLanguageSeparation;
    }
  }
}

// static
StatusAreaView::OpenTabsMode StatusAreaView::GetOpenTabsMode() {
  return open_tabs_mode_;
}

// static
void StatusAreaView::SetOpenTabsMode(OpenTabsMode mode) {
  open_tabs_mode_ = mode;
}

}  // namespace chromeos
