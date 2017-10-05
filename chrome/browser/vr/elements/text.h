// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_H_

#include <memory>

#include "chrome/browser/vr/elements/textured_element.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class TextTexture;
class UiTexture;

class Text : public TexturedElement {
 public:
  Text(int maximum_width_pixels,
       float font_height_meters,
       float text_width_meters,
       const base::string16& text);
  ~Text() override;

  void SetColor(SkColor color);

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<TextTexture> texture_;
  DISALLOW_COPY_AND_ASSIGN(Text);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
