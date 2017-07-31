// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SCREEN_DIMMER_H_
#define CHROME_BROWSER_VR_ELEMENTS_SCREEN_DIMMER_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

class ScreenDimmer : public UiElement {
 public:
  ScreenDimmer();
  ~ScreenDimmer() override;

  void Initialize() final;

  // UiElement interface.
  void Render(UiElementRenderer* renderer,
              const gfx::Transform& view_proj_matrix) const final;

  DISALLOW_COPY_AND_ASSIGN(ScreenDimmer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SCREEN_DIMMER_H_
