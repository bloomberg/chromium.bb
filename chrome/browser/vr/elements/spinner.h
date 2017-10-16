// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SPINNER_H_
#define CHROME_BROWSER_VR_ELEMENTS_SPINNER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class SpinnerTexture;

class Spinner : public TexturedElement {
 public:
  explicit Spinner(int maximum_width);
  ~Spinner() override;

  void SetColor(SkColor color);

 protected:
  UiTexture* GetTexture() const override;

 private:
  void NotifyClientFloatAnimated(float value,
                                 int target_property_id,
                                 cc::Animation* animation) override;
  std::unique_ptr<SpinnerTexture> texture_;
  DISALLOW_COPY_AND_ASSIGN(Spinner);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SPINNER_H_
