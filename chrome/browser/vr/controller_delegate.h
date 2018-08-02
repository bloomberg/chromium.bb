// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_VR_CONTROLLER_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace vr {

class InputEvent;
struct ControllerModel;
struct RenderInfo;

using InputEventList = std::vector<std::unique_ptr<InputEvent>>;

// Communicates with the PlatformController to update it and obtain input and
// movement information.
class ControllerDelegate {
 public:
  virtual ~ControllerDelegate() {}

  virtual void UpdateController(const RenderInfo& render_info,
                                base::TimeTicks current_time,
                                bool is_webxr_frame) = 0;
  virtual ControllerModel GetModel(const RenderInfo& render_info) = 0;
  virtual InputEventList GetGestures(base::TimeTicks current_time) = 0;
  virtual device::mojom::XRInputSourceStatePtr GetInputSourceState() = 0;
  virtual void OnResume() = 0;
  virtual void OnPause() = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CONTROLLER_DELEGATE_H_
