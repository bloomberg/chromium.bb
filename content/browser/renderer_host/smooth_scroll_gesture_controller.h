// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SMOOTH_SCROLL_GESTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_SMOOTH_SCROLL_GESTURE_CONTROLLER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/common/content_export.h"

struct ViewHostMsg_BeginSmoothScroll_Params;

namespace content {

class RenderWidgetHost;
class RenderWidgetHostViewPort;
class SmoothScrollGesture;

// Controls SmoothScrollGestures, used to inject synthetic events
// for performance test harness.
class CONTENT_EXPORT SmoothScrollGestureController {
 public:
  SmoothScrollGestureController();
  ~SmoothScrollGestureController();

  // Initiates a synthetic event stream.
  void BeginSmoothScroll(RenderWidgetHostViewPort* view,
                         const ViewHostMsg_BeginSmoothScroll_Params& params);

  base::TimeDelta GetSyntheticScrollMessageInterval() const;

 private:
  // Called periodically to advance the active scroll gesture after being
  // initiated by OnBeginSmoothScroll.
  void OnTimer();

  base::RepeatingTimer<SmoothScrollGestureController> timer_;

  RenderWidgetHost* rwh_;

  scoped_refptr<SmoothScrollGesture> pending_smooth_scroll_gesture_;

  DISALLOW_COPY_AND_ASSIGN(SmoothScrollGestureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SMOOTH_SCROLL_GESTURE_CONTROLLER_H_
