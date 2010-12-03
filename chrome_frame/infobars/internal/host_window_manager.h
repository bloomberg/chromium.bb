// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_INFOBARS_INTERNAL_HOST_WINDOW_MANAGER_H_
#define CHROME_FRAME_INFOBARS_INTERNAL_HOST_WINDOW_MANAGER_H_

#include "base/basictypes.h"
#include "chrome_frame/infobars/internal/subclassing_window_with_delegate.h"

class DisplacedWindowManager;

// HostWindowManager observes the HWND passed to Initialize and:
// 1) Monitors the lifecycle of a specific child window (as identified by
//    FindDisplacedWindow).
// 2) Intercepts NCCALCSIZE events on the child window, allowing the client to
//    modify the child window's requested dimensions.
// 3) Allows the client to request a recalculation of the child window's
//    dimensions (resulting in a callback as in [2]).
//
// See documentation of SubclasingWindowWithDelegate for further information.
class HostWindowManager
    : public SubclassingWindowWithDelegate<HostWindowManager> {
 public:
  HostWindowManager();
  virtual ~HostWindowManager();

  // Triggers an immediate re-evaluation of the dimensions of the displaced
  // window. Delegate::AdjustDisplacedWindowDimensions will be called with the
  // natural dimensions of the displaced window.
  void UpdateLayout();

 private:
  class DisplacedWindowDelegate;
  friend class DisplacedWindowDelegate;

  // Finds the window to be displaced and instantiate a DisplacedWindowManager
  // for it if one does not already exist. Returns true if
  // displaced_window_manager_ is non-NULL at the end of the call.
  bool FindDisplacedWindow(HWND old_window);

  // Subclasses and observes changes to the displaced window.
  DisplacedWindowManager* displaced_window_manager_;

  DISALLOW_COPY_AND_ASSIGN(HostWindowManager);
};  // class HostWindowManager

#endif  // CHROME_FRAME_INFOBARS_INTERNAL_HOST_WINDOW_MANAGER_H_
