// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ZOOM_ZOOM_OBSERVER_H_
#define CHROME_BROWSER_UI_ZOOM_ZOOM_OBSERVER_H_

#include "chrome/browser/ui/zoom/zoom_controller.h"

namespace content {
class WebContents;
}

// Interface for objects that wish to be notified of changes in ZoomController.
class ZoomObserver {
 public:
  // Notification that the zoom percentage has changed.
  virtual void OnZoomChanged(const ZoomController::ZoomChangedEventData& data) {
  }

 protected:
  virtual ~ZoomObserver() {}
};

#endif  // CHROME_BROWSER_UI_ZOOM_ZOOM_OBSERVER_H_
