// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class TextInputTexture;

// TODO(cjgrant): This class must be refactored to reuse Text and Rect elements
// for the text and cursor. It exists as-is to facilitate initial integration of
// the keyboard and omnibox.
class TextInput : public TexturedElement {
 public:
  TextInput(int maximum_width_pixels,
            float font_height_meters,
            float text_width_meters);
  ~TextInput() override;

  void SetText(const base::string16& text);
  void SetCursorPosition(int position);
  void SetColor(SkColor color);

  typedef base::Callback<void(const base::string16& text)> TextInputCallback;
  void SetTextChangedCallback(const TextInputCallback& callback);

  bool OnBeginFrame(const base::TimeTicks& time,
                    const gfx::Vector3dF& look_at) override;

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<TextInputTexture> texture_;
  TextInputCallback text_changed_callback_;
  base::string16 text_;

  DISALLOW_COPY_AND_ASSIGN(TextInput);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_
