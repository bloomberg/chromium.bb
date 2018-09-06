// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SCHEDULER_RENDER_LOOP_INTERFACE_H_
#define CHROME_BROWSER_VR_SCHEDULER_RENDER_LOOP_INTERFACE_H_

namespace base {
class TimeTicks;
}

namespace vr {

class SchedulerRenderLoopInterface {
 public:
  virtual ~SchedulerRenderLoopInterface() {}
  virtual void DrawBrowserFrame(base::TimeTicks current_time) = 0;
  virtual void DrawWebXrFrame(base::TimeTicks current_time) = 0;
  virtual void ProcessControllerInputForWebXr(base::TimeTicks current_time) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SCHEDULER_RENDER_LOOP_INTERFACE_H_
