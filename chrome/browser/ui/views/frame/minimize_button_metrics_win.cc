// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/minimize_button_metrics_win.h"

#include "base/logging.h"
#include "base/i18n/rtl.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/win/dpi.h"

namespace {

int GetMinimizeButtonOffsetForWindow(HWND hwnd) {
  // The WM_GETTITLEBARINFOEX message can fail if we are not active/visible. By
  // fail we get a location of 0; the return status code is always the same and
  // similarly the state never seems to change (titlebar_info.rgstate).
  TITLEBARINFOEX titlebar_info = {0};
  titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(hwnd, WM_GETTITLEBARINFOEX, 0,
              reinterpret_cast<WPARAM>(&titlebar_info));

  if (titlebar_info.rgrect[2].left == titlebar_info.rgrect[2].right ||
      (titlebar_info.rgstate[2] & (STATE_SYSTEM_INVISIBLE |
                                   STATE_SYSTEM_OFFSCREEN |
                                   STATE_SYSTEM_UNAVAILABLE))) {
    return 0;
  }

  // WM_GETTITLEBARINFOEX returns rects in screen coordinates in pixels.
  // We need to convert the minimize button corner offset to DIP before
  // returning it.
  POINT minimize_button_corner = { titlebar_info.rgrect[2].left, 0 };
  MapWindowPoints(HWND_DESKTOP, hwnd, &minimize_button_corner, 1);
  return minimize_button_corner.x / gfx::win::GetDeviceScaleFactor();
}

}  // namespace

// static
int MinimizeButtonMetrics::last_cached_minimize_button_x_delta_ = 0;

MinimizeButtonMetrics::MinimizeButtonMetrics()
    : hwnd_(NULL),
      cached_minimize_button_x_delta_(last_cached_minimize_button_x_delta_),
      was_activated_(false) {
}

MinimizeButtonMetrics::~MinimizeButtonMetrics() {
}

void MinimizeButtonMetrics::Init(HWND hwnd) {
  DCHECK(!hwnd_);
  hwnd_ = hwnd;
}

void MinimizeButtonMetrics::OnHWNDActivated() {
  was_activated_ = true;
  // NOTE: we don't cache here as it seems only after the activate is the value
  // correct.
}

int MinimizeButtonMetrics::GetMinimizeButtonOffsetX() const {
  // Under DWM WM_GETTITLEBARINFOEX won't return the right thing until after
  // WM_NCACTIVATE (maybe it returns classic values?). In an attempt to return a
  // consistant value we cache the last value across instances and use it until
  // we get the activate.
  if (was_activated_ || !ui::win::IsAeroGlassEnabled() ||
      cached_minimize_button_x_delta_ == 0) {
    const int minimize_button_offset = GetAndCacheMinimizeButtonOffsetX();
    if (minimize_button_offset > 0)
      return minimize_button_offset;
  }

  // If we fail to get the minimize button offset via the WM_GETTITLEBARINFOEX
  // message then calculate and return this via the
  // cached_minimize_button_x_delta_ member value. Please see
  // CacheMinimizeButtonDelta() for more details.
  DCHECK(cached_minimize_button_x_delta_);

  if (base::i18n::IsRTL())
    return cached_minimize_button_x_delta_;

  RECT client_rect = {0};
  GetClientRect(hwnd_, &client_rect);
  return client_rect.right - cached_minimize_button_x_delta_;
}

int MinimizeButtonMetrics::GetAndCacheMinimizeButtonOffsetX() const {
  const int minimize_button_offset = GetMinimizeButtonOffsetForWindow(hwnd_);
  if (minimize_button_offset <= 0)
    return 0;

  if (base::i18n::IsRTL()) {
    cached_minimize_button_x_delta_ = minimize_button_offset;
  } else {
    RECT client_rect = {0};
    GetClientRect(hwnd_, &client_rect);
    cached_minimize_button_x_delta_ =
        client_rect.right - minimize_button_offset;
  }
  last_cached_minimize_button_x_delta_ = cached_minimize_button_x_delta_;
  return minimize_button_offset;
}
