// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class TextInputTexture;

// TODO(cjgrant): This class must be refactored to reuse Text and Rect elements
// for the text and cursor. It exists as-is to facilitate initial integration of
// the keyboard and omnibox.
class TextInput : public TexturedElement {
 public:
  // Called when this element recieves focus.
  typedef base::RepeatingCallback<void(bool)> OnFocusChangedCallback;
  // Called when the user enters text while this element is focused.
  typedef base::RepeatingCallback<void(const TextInputInfo&)>
      OnInputEditedCallback;
  TextInput(int maximum_width_pixels,
            float font_height_meters,
            float text_width_meters,
            OnFocusChangedCallback focus_changed_callback,
            OnInputEditedCallback input_edit_callback);
  ~TextInput() override;

  bool IsEditable() override;
  void OnButtonUp(const gfx::PointF& position) override;
  void OnFocusChanged(bool focused) override;
  void OnInputEdited(const TextInputInfo& info) override;
  void OnInputCommitted(const TextInputInfo& info) override;

  void SetTextInputDelegate(TextInputDelegate* text_input_delegate);
  void SetColor(SkColor color);
  void UpdateInput(const TextInputInfo& info);

  bool OnBeginFrame(const base::TimeTicks& time,
                    const gfx::Vector3dF& look_at) override;

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<TextInputTexture> texture_;
  OnFocusChangedCallback focus_changed_callback_;
  OnInputEditedCallback input_edit_callback_;
  TextInputDelegate* delegate_ = nullptr;
  TextInputInfo text_info_;
  bool focused_ = false;

  DISALLOW_COPY_AND_ASSIGN(TextInput);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_
