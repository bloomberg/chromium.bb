// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_INVISIBLE_HIT_TARGET_H_
#define CHROME_BROWSER_VR_ELEMENTS_INVISIBLE_HIT_TARGET_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

// A hit target does not render, but may affect reticle positioning.
class InvisibleHitTarget : public UiElement {
 public:
  InvisibleHitTarget();
  ~InvisibleHitTarget() override;

  void Render(UiElementRenderer* renderer,
              const gfx::Transform& view_proj_matrix) const final;

 private:
  DISALLOW_COPY_AND_ASSIGN(InvisibleHitTarget);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_INVISIBLE_HIT_TARGET_H_
