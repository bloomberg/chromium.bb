// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXTURED_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXTURED_ELEMENT_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

class SkSurface;

namespace vr {

class UiTexture;

class TexturedElement : public UiElement {
 public:
  // |preferred_width| is the element's desired width in meters. Constraints
  // implied by the texture being rendered may or may not allow it to be
  // rendered exactly at the preferred width.
  explicit TexturedElement(int maximum_width);

  ~TexturedElement() override;

  void Initialize(SkiaSurfaceProvider* provider) final;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;

  static void SetInitializedForTesting();
  static void SetRerenderIfNotDirtyForTesting();

  // Foreground and background colors are used pervasively in textured elements,
  // but more element-specific colors should be set on the appropriate element.
  void SetForegroundColor(SkColor color);
  void SetBackgroundColor(SkColor color);

 protected:
  virtual UiTexture* GetTexture() const = 0;
  virtual void UpdateElementSize();

  bool PrepareToDraw() final;

 private:
  gfx::Size texture_size_;
  GLuint texture_handle_ = 0;
  int maximum_width_;
  bool initialized_ = false;

  sk_sp<SkSurface> surface_;
  SkiaSurfaceProvider* provider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TexturedElement);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXTURED_ELEMENT_H_
