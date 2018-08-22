// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_COMPOSITOR_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_COMPOSITOR_UI_INTERFACE_H_

#include <utility>

#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

struct FovRectangle {
  float left;
  float right;
  float bottom;
  float top;
};

using FovRectangles = std::pair<FovRectangle, FovRectangle>;

class VR_EXPORT CompositorUiInterface {
 public:
  virtual ~CompositorUiInterface() {}

  virtual void OnGlInitialized(unsigned int content_texture_id,
                               GlTextureLocation content_location,
                               unsigned int content_overlay_texture_id,
                               GlTextureLocation content_overlay_location,
                               unsigned int ui_texture_id) = 0;

  virtual void OnWebXrFrameAvailable() = 0;
  virtual void OnWebXrTimedOut() = 0;
  virtual void OnWebXrTimeoutImminent() = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_COMPOSITOR_UI_INTERFACE_H_
