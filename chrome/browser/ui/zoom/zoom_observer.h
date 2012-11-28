// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ZOOM_ZOOM_OBSERVER_H_
#define CHROME_BROWSER_UI_ZOOM_ZOOM_OBSERVER_H_

#include "base/debug/stack_trace.h"

namespace content {
class WebContents;
}

// TODO(eroman): Temporary for bug 144879. Defined in zoom_controller.cc.
class HelperForBug144879 {
 public:
  HelperForBug144879();
  ~HelperForBug144879();
  void CheckIsAlive();

 private:
  enum Status {
    ALIVE = 0xCA11AB1E,
    DEAD = 0xB100D1ED,
  };

  base::debug::StackTrace deletion_callstack_;
  Status status_;
};

// Interface for objects that wish to be notified of changes in ZoomController.
class ZoomObserver {
 public:
  // Notification that the zoom percentage has changed.
  virtual void OnZoomChanged(content::WebContents* source,
                             bool can_show_bubble) = 0;

  void CheckIsAliveForBug144879() {
    bug144879_.CheckIsAlive();
  }

 protected:
  virtual ~ZoomObserver() {}

 private:
  HelperForBug144879 bug144879_;
};

#endif  // CHROME_BROWSER_UI_ZOOM_ZOOM_OBSERVER_H_
