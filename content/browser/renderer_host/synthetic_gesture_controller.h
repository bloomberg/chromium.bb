// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SYNTHETIC_GESTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_SYNTHETIC_GESTURE_CONTROLLER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

struct ViewHostMsg_BeginPinch_Params;
struct ViewHostMsg_BeginSmoothScroll_Params;

namespace content {

class RenderWidgetHost;
class RenderWidgetHostViewPort;
class SyntheticGesture;

// Controls SyntheticGestures, used to inject synthetic events
// for performance test harness.
class CONTENT_EXPORT SyntheticGestureController {
 public:
  SyntheticGestureController();
  ~SyntheticGestureController();

  // Initiates a synthetic event stream to simulate a smooth scroll.
  void BeginSmoothScroll(RenderWidgetHostViewPort* view,
                         const ViewHostMsg_BeginSmoothScroll_Params& params);

  // Initiates a synthetic event stream to simulate a pinch-to-zoom.
  void BeginPinch(RenderWidgetHostViewPort* view,
                  const ViewHostMsg_BeginPinch_Params& params);

  base::TimeDelta GetSyntheticGestureMessageInterval() const;

 private:
  // Called periodically to advance the active scroll gesture after being
  // initiated by OnBeginSmoothScroll.
  void OnTimer();

  base::RepeatingTimer<SyntheticGestureController> timer_;

  RenderWidgetHost* rwh_;

  scoped_refptr<SyntheticGesture> pending_synthetic_gesture_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticGestureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SYNTHETIC_GESTURE_CONTROLLER_H_
