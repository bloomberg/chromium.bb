// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENT_RENDERER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENT_RENDERER_H_

#include "device/vr/vr_types.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr_shell {

// This is the interface offered by VrShell's GL system to UI elements.
class UiElementRenderer {
 public:
  virtual ~UiElementRenderer() {}

  virtual void DrawTexturedQuad(int texture_data_handle,
                                const vr::Mat4f& view_proj_matrix,
                                const gfx::RectF& copy_rect,
                                float opacity) = 0;

  virtual void DrawGradientQuad(const vr::Mat4f& view_proj_matrix,
                                const SkColor edge_color,
                                const SkColor center_color,
                                float opacity) = 0;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENT_RENDERER_H_
