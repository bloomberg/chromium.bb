// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDER_LOOP_H_
#define CHROME_BROWSER_VR_RENDER_LOOP_H_

#include <memory>

#include "chrome/browser/vr/vr_export.h"

namespace base {
class TimeTicks;
}

namespace vr {

class ControllerDelegate;
class UiInterface;
struct RenderInfo;

// This abstract class handles all input/output activities during a frame.
// This includes head movement, controller movement and input, audio output and
// rendering of the frame.
// TODO(acondor): Move more functionality cross platform functionality from
// VrShellGl and make this class concrete (http://crbug.com/767282).
class VR_EXPORT RenderLoop {
 public:
  explicit RenderLoop(std::unique_ptr<UiInterface> ui);
  virtual ~RenderLoop();

 protected:
  void ProcessControllerInput(const RenderInfo& render_info,
                              base::TimeTicks current_time,
                              bool is_webxr_frame);

  std::unique_ptr<UiInterface> ui_;
  std::unique_ptr<ControllerDelegate> controller_delegate_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDER_LOOP_H_
