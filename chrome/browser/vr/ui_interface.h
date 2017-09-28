// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_UI_INTERFACE_H_

namespace gfx {
class Transform;
}

namespace vr {

// This is the platform-specific interface to the VR UI.
class UiInterface {
 public:
  enum Direction {
    NONE = 0,
    LEFT,
    RIGHT,
    UP,
    DOWN,
  };

  virtual ~UiInterface() {}

  virtual bool ShouldRenderWebVr() = 0;

  virtual void OnGlInitialized(unsigned int content_texture_id) = 0;
  virtual void OnAppButtonClicked() = 0;
  virtual void OnAppButtonGesturePerformed(
      UiInterface::Direction direction) = 0;
  virtual void OnProjMatrixChanged(const gfx::Transform& proj_matrix) = 0;
  virtual void OnWebVrFrameAvailable() = 0;
  virtual void OnWebVrTimedOut() = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INTERFACE_H_
