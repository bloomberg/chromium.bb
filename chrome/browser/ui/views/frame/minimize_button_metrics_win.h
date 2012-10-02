// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MINIMIZE_BUTTON_METRICS_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MINIMIZE_BUTTON_METRICS_WIN_H_

#include <windows.h>

#include "base/basictypes.h"

// Class that implements obtaining the X coordinate of the native minimize
// button for the native frame on Windows.
// This is a separate class because obtaining it is somewhat tricky and this
// code is shared between BrowserDesktopRootWindowHostWin and BrowserFrameWin.
class MinimizeButtonMetrics {
 public:
  MinimizeButtonMetrics();
  ~MinimizeButtonMetrics();

  void Init(HWND hwnd);

  // Obtain the X offset of the native minimize button. Since Windows can lie
  // to us if we call this at the wrong moment, this might come from a cached
  // value rather than read when called.
  int GetMinimizeButtonOffsetX() const;

  // Must be called when hwnd_ is activated to update the minimize button
  // position cache.
  void OnHWNDActivated();

 private:
  HWND hwnd_;
  int cached_minimize_button_x_delta_;

  DISALLOW_COPY_AND_ASSIGN(MinimizeButtonMetrics);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MINIMIZE_BUTTON_METRICS_WIN_H_
