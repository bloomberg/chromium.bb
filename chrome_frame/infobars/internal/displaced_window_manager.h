// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_INFOBARS_INTERNAL_DISPLACED_WINDOW_MANAGER_H_
#define CHROME_FRAME_INFOBARS_INTERNAL_DISPLACED_WINDOW_MANAGER_H_

#include <atlbase.h>
#include <atlcrack.h>
#include <atlwin.h>

#include "base/basictypes.h"
#include "chrome_frame/infobars/internal/subclassing_window_with_delegate.h"

// DisplacedWindowManager observes the HWND passed to Initialize and:
// 1) Intercepts NCCALCSIZE events allowing the client to modify the window's
//    requested dimensions.
// 2) Allows the client to request a recalculation of the window's dimensions
//    (resulting in a deferred callback as in [1]).
// 3) Is destroyed only when the window is destroyed.
class DisplacedWindowManager
    : public SubclassingWindowWithDelegate<DisplacedWindowManager> {
 public:
  DisplacedWindowManager();

  // Triggers an immediate re-evaluation of the dimensions of the displaced
  // window. Delegate::AdjustDisplacedWindowDimensions will be called with the
  // natural dimensions of the displaced window.
  void UpdateLayout();

  BEGIN_MSG_MAP_EX(DisplacedWindowManager)
    MSG_WM_NCCALCSIZE(OnNcCalcSize)
    CHAIN_MSG_MAP(SubclassingWindowWithDelegate<DisplacedWindowManager>)
  END_MSG_MAP()

 private:
  // The size of the displaced window is being calculated. Allow
  // InfobarWindows to reserve a part of the space for themselves, if they are
  // visible.
  LRESULT OnNcCalcSize(BOOL calc_valid_rects, LPARAM lparam);

  DISALLOW_COPY_AND_ASSIGN(DisplacedWindowManager);
};  // class DisplacedWindowManager

#endif  // CHROME_FRAME_INFOBARS_INTERNAL_DISPLACED_WINDOW_MANAGER_H_
