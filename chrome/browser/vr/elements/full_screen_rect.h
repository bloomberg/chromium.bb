// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_FULL_SCREEN_RECT_H_
#define CHROME_BROWSER_VR_ELEMENTS_FULL_SCREEN_RECT_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/rect.h"

namespace vr {

// A FullScreenRect ignores its inherited transform and fills the entire
// viewport.
class FullScreenRect : public Rect {
 public:
  FullScreenRect();
  ~FullScreenRect() override;

 private:
  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;
  bool IsWorldPositioned() const final;

  DISALLOW_COPY_AND_ASSIGN(FullScreenRect);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_FULL_SCREEN_RECT_H_
