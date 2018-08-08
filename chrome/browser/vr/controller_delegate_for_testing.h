// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CONTROLLER_DELEGATE_FOR_TESTING_H_
#define CHROME_BROWSER_VR_CONTROLLER_DELEGATE_FOR_TESTING_H_

#include <queue>

#include "base/macros.h"
#include "chrome/browser/vr/controller_delegate.h"
#include "chrome/browser/vr/model/controller_model.h"

namespace vr {

class UiInterface;
struct ControllerTestInput;

class ControllerDelegateForTesting : public ControllerDelegate {
 public:
  explicit ControllerDelegateForTesting(UiInterface* ui);
  ~ControllerDelegateForTesting() override;

  void QueueControllerActionForTesting(ControllerTestInput controller_input);
  bool IsQueueEmpty() const;

  // ControllerDelegate implementation.
  void UpdateController(const RenderInfo& render_info,
                        base::TimeTicks current_time,
                        bool is_webxr_frame) override;
  ControllerModel GetModel(const RenderInfo& render_info) override;
  InputEventList GetGestures(base::TimeTicks current_time) override;
  device::mojom::XRInputSourceStatePtr GetInputSourceState() override;
  void OnResume() override;
  void OnPause() override;

 private:
  UiInterface* ui_;
  std::queue<ControllerModel> controller_model_queue_;
  ControllerModel cached_controller_model_;

  DISALLOW_COPY_AND_ASSIGN(ControllerDelegateForTesting);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CONTROLLER_DELEGATE_FOR_TESTING_H_
