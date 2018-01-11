// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TESTAPP_VR_TEST_CONTEXT_H_
#define CHROME_BROWSER_VR_TESTAPP_VR_TEST_CONTEXT_H_

#include "base/macros.h"

#include <cstdint>

#include "base/time/time.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "ui/gfx/transform.h"

namespace ui {
class Event;
}

namespace vr {

class TextInputDelegate;
class TestKeyboardDelegate;
class Ui;
struct Model;

// This class provides a home for the VR UI in a testapp context, and
// manipulates the UI according to user input.
class VrTestContext : public vr::UiBrowserInterface {
 public:
  VrTestContext();
  ~VrTestContext() override;

  void OnGlInitialized();
  // TODO(vollick): we should refactor VrShellGl's rendering logic and use it
  // directly. crbug.com/767282
  void DrawFrame();
  void HandleInput(ui::Event* event);

  // vr::UiBrowserInterface implementation (UI calling to VrShell).
  void ExitPresent() override;
  void ExitFullscreen() override;
  void NavigateBack() override;
  void ExitCct() override;
  void OnUnsupportedMode(vr::UiUnsupportedMode mode) override;
  void OnExitVrPromptResult(vr::ExitVrPromptChoice choice,
                            vr::UiUnsupportedMode reason) override;
  void OnContentScreenBoundsChanged(const gfx::SizeF& bounds) override;
  void SetVoiceSearchActive(bool active) override;
  void StartAutocomplete(const base::string16& string) override;
  void StopAutocomplete() override;
  void Navigate(GURL gurl) override;
  void LoadAssets() override;

  void set_window_size(const gfx::Size& size) { window_size_ = size; }

 private:
  unsigned int CreateFakeContentTexture();
  void CreateFakeOmniboxSuggestions();
  void CreateFakeVoiceSearchResult();
  void CreateFakeTextInputOrCommit(bool commit);
  void CycleWebVrModes();
  void ToggleSplashScreen();
  void CycleOrigin();
  gfx::Transform ProjectionMatrix() const;
  gfx::Transform ViewProjectionMatrix() const;
  ControllerModel UpdateController();

  std::unique_ptr<Ui> ui_;
  gfx::Size window_size_;

  gfx::Transform head_pose_;
  float head_angle_x_degrees_ = 0;
  float head_angle_y_degrees_ = 0;
  int last_drag_x_pixels_ = 0;
  int last_drag_y_pixels_ = 0;
  gfx::Point last_mouse_point_;
  bool touchpad_pressed_ = false;

  float view_scale_factor_ = 1.f;

  // This avoids storing a duplicate of the model state here.
  Model* model_;

  bool fullscreen_ = false;
  bool incognito_ = false;
  bool show_web_vr_splash_screen_ = false;
  bool voice_search_enabled_ = false;
  base::TimeTicks page_load_start_;

  ControllerModel last_controller_model_;

  std::unique_ptr<TextInputDelegate> text_input_delegate_;
  std::unique_ptr<TestKeyboardDelegate> keyboard_delegate_;

  DISALLOW_COPY_AND_ASSIGN(VrTestContext);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TESTAPP_VR_TEST_CONTEXT_H_
