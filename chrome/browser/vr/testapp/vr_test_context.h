// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TESTAPP_VR_TEST_CONTEXT_H_
#define CHROME_BROWSER_VR_TESTAPP_VR_TEST_CONTEXT_H_

#include "base/macros.h"

#include <cstdint>

#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "ui/gfx/transform.h"

namespace ui {
class Event;
}

namespace vr {

struct ControllerInfo;
class Ui;

// This class provides a home for the VR UI in a testapp context, and
// manipulates the UI according to user input.
class VrTestContext : public vr::ContentInputDelegate,
                      public vr::UiBrowserInterface {
 public:
  VrTestContext();
  ~VrTestContext() override;

  void OnGlInitialized(const gfx::Size& window_size);
  void DrawFrame();
  void HandleInput(ui::Event* event);

  // vr::ContentInputDelegate.
  void OnContentEnter(const gfx::PointF& normalized_hit_point) override;
  void OnContentLeave() override;
  void OnContentMove(const gfx::PointF& normalized_hit_point) override;
  void OnContentDown(const gfx::PointF& normalized_hit_point) override;
  void OnContentUp(const gfx::PointF& normalized_hit_point) override;
  void OnContentFlingStart(std::unique_ptr<blink::WebGestureEvent> gesture,
                           const gfx::PointF& normalized_hit_point) override;
  void OnContentFlingCancel(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override;
  void OnContentScrollBegin(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override;
  void OnContentScrollUpdate(std::unique_ptr<blink::WebGestureEvent> gesture,
                             const gfx::PointF& normalized_hit_point) override;
  void OnContentScrollEnd(std::unique_ptr<blink::WebGestureEvent> gesture,
                          const gfx::PointF& normalized_hit_point) override;
  void SetVoiceSearchActive(bool active) override;

  // vr::UiBrowserInterface implementation (UI calling to VrShell).
  void ExitPresent() override;
  void ExitFullscreen() override;
  void NavigateBack() override;
  void ExitCct() override;
  void OnUnsupportedMode(vr::UiUnsupportedMode mode) override;
  void OnExitVrPromptResult(vr::UiUnsupportedMode reason,
                            vr::ExitVrPromptChoice choice) override;
  void OnContentScreenBoundsChanged(const gfx::SizeF& bounds) override;

 private:
  unsigned int CreateFakeContentTexture();

  std::unique_ptr<Ui> ui_;
  gfx::Size window_size_;

  std::unique_ptr<ControllerInfo> controller_info_;

  gfx::Transform head_pose_;
  float head_angle_x_degrees_ = 0;
  float head_angle_y_degrees_ = 0;
  int last_drag_x_pixels_ = 0;
  int last_drag_y_pixels_ = 0;

  float view_scale_factor_ = 1.f;

  bool fullscreen_ = false;
  bool incognito_ = false;

  DISALLOW_COPY_AND_ASSIGN(VrTestContext);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TESTAPP_VR_TEST_CONTEXT_H_
