// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/infobars/internal/displaced_window_manager.h"

DisplacedWindowManager::DisplacedWindowManager() {
}

void DisplacedWindowManager::UpdateLayout() {
  // Call SetWindowPos with SWP_FRAMECHANGED for displaced window.
  // Displaced window will receive WM_NCCALCSIZE to recalculate its client size.
  ::SetWindowPos(m_hWnd,
                 NULL, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_FRAMECHANGED);
}

LRESULT DisplacedWindowManager::OnNcCalcSize(BOOL calc_valid_rects,
                                             LPARAM lparam) {
  // Ask the original window proc to calculate the 'natural' size of the window.
  LRESULT ret = DefWindowProc(WM_NCCALCSIZE,
                              static_cast<WPARAM>(calc_valid_rects), lparam);
  if (lparam == NULL)
    return ret;

  // Whether calc_valid_rects is true or false, we could treat beginning of
  // lparam as a RECT object.
  RECT* rect = reinterpret_cast<RECT*>(lparam);
  RECT natural_rect = *rect;
  if (delegate() != NULL)
    delegate()->AdjustDisplacedWindowDimensions(rect);

  // If we modified the window's dimensions, there is no way to respect a custom
  // "client-area preservation strategy", so we must force a redraw to be safe.
  if (calc_valid_rects && ret == WVR_VALIDRECTS &&
      !EqualRect(&natural_rect, rect)) {
    ret = WVR_REDRAW;
  }

  return ret;
}
