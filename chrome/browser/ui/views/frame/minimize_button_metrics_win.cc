// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/minimize_button_metrics_win.h"

#include "base/logging.h"
#include "base/i18n/rtl.h"

namespace {

int GetMinimizeButtonOffsetForWindow(HWND hwnd) {
  // The WM_GETTITLEBARINFOEX message can fail if we are not active/visible.
  TITLEBARINFOEX titlebar_info = {0};
  titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(hwnd, WM_GETTITLEBARINFOEX, 0,
              reinterpret_cast<WPARAM>(&titlebar_info));

  POINT minimize_button_corner = { titlebar_info.rgrect[2].left,
                                   titlebar_info.rgrect[2].top };
  MapWindowPoints(HWND_DESKTOP, hwnd, &minimize_button_corner, 1);
  return minimize_button_corner.x;
}

}  // namespace

MinimizeButtonMetrics::MinimizeButtonMetrics()
    : hwnd_(NULL),
      cached_minimize_button_x_delta_(0) {
}

MinimizeButtonMetrics::~MinimizeButtonMetrics() {
}

void MinimizeButtonMetrics::Init(HWND hwnd) {
  DCHECK(!hwnd_);
  hwnd_ = hwnd;
}

void MinimizeButtonMetrics::OnHWNDActivated() {
  int minimize_offset = GetMinimizeButtonOffsetForWindow(hwnd_);

  RECT rect = {0};
  GetClientRect(hwnd_, &rect);
  // Calculate and cache the value of the minimize button delta, i.e. the
  // offset to be applied to the left or right edge of the client rect
  // depending on whether the language is RTL or not.
  // This cached value is only used if the WM_GETTITLEBARINFOEX message fails
  // to get the offset of the minimize button.
  if (base::i18n::IsRTL())
    cached_minimize_button_x_delta_ = minimize_offset;
  else
    cached_minimize_button_x_delta_ = rect.right - minimize_offset;
}

int MinimizeButtonMetrics::GetMinimizeButtonOffsetX() const {
  int minimize_button_offset = GetMinimizeButtonOffsetForWindow(hwnd_);
  if (minimize_button_offset > 0)
    return minimize_button_offset;

  // If we fail to get the minimize button offset via the WM_GETTITLEBARINFOEX
  // message then calculate and return this via the
  // cached_minimize_button_x_delta_ member value. Please see
  // CacheMinimizeButtonDelta() for more details.
  DCHECK(cached_minimize_button_x_delta_);

  RECT client_rect = {0};
  GetClientRect(hwnd_, &client_rect);

  if (base::i18n::IsRTL())
    return cached_minimize_button_x_delta_;
  return client_rect.right - cached_minimize_button_x_delta_;
}

