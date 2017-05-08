// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_TEXTURED_ELEMENT_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_TEXTURED_ELEMENT_H_

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"
#include "ui/gl/gl_bindings.h"

class SkSurface;

namespace vr_shell {

class UiTexture;

class TexturedElement : public UiElement {
 public:
  // |preferred_width| is the element's desired width in meters. Constraints
  // implied by the texture being rendered may or may not allow it to be
  // rendered exactly at the preferred width.
  explicit TexturedElement(int maximum_width);
  ~TexturedElement() override;

  void Initialize() final;

  // UiElement interface.
  void Render(VrShellRenderer* renderer,
              vr::Mat4f view_proj_matrix) const final;

 protected:
  virtual UiTexture* GetTexture() const = 0;
  void Update();

 private:
  void Flush(SkSurface* surface);

  gfx::Size texture_size_;
  GLuint texture_handle_;
  int maximum_width_;

  DISALLOW_COPY_AND_ASSIGN(TexturedElement);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_TEXTURED_ELEMENT_H_
