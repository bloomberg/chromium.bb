// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SYSTEM_INDICATOR_H_
#define CHROME_BROWSER_VR_ELEMENTS_SYSTEM_INDICATOR_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class UiTexture;

class SystemIndicator : public TexturedElement {
 public:
  explicit SystemIndicator(int preferred_width,
                           float heigh_meters,
                           const gfx::VectorIcon& icon,
                           int string_id);
  ~SystemIndicator() override;

 protected:
  void UpdateElementSize() override;

 private:
  UiTexture* GetTexture() const override;
  std::unique_ptr<UiTexture> texture_;
  float height_meters_;

  DISALLOW_COPY_AND_ASSIGN(SystemIndicator);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SYSTEM_INDICATOR_H_
