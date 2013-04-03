// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SMOOTH_GESTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_SMOOTH_GESTURE_CONTROLLER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/time.h"
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
  bool OnBeginSmoothScroll(
      RenderWidgetHostViewPort* view,
      const ViewHostMsg_BeginSmoothScroll_Params& params);

  // Called when an input event is going to be forwarded to the renderer.
  void OnForwardInputEvent();

  // Called when the |rwh| received an ACK for an input event.
  void OnInputEventACK(RenderWidgetHost* rwh);

  int SyntheticScrollMessageInterval() const;

 private:
  // Called periodically to advance the active scroll gesture after being
  // initiated by OnBeginSmoothScroll.
  void TickActiveSmoothScrollGesture(RenderWidgetHost* rwh);

  base::WeakPtrFactory<SmoothScrollGestureController> weak_factory_;

  int pending_input_event_count_;

  scoped_refptr<SmoothScrollGesture> pending_smooth_scroll_gesture_;

  base::TimeTicks last_smooth_scroll_gesture_tick_time_;
  bool tick_active_smooth_scroll_gesture_task_posted_;

  DISALLOW_COPY_AND_ASSIGN(SmoothScrollGestureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SMOOTH_GESTURE_CONTROLLER_H_
