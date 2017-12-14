// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class Rect;
class Text;

class TextInput : public UiElement {
 public:
  // Called when this element recieves focus.
  typedef base::RepeatingCallback<void(bool)> OnFocusChangedCallback;
  // Called when the user enters text while this element is focused.
  typedef base::RepeatingCallback<void(const TextInputInfo&)>
      OnInputEditedCallback;
  // Called when the user commits text while this element is focused.
  typedef base::RepeatingCallback<void(const TextInputInfo&)>
      OnInputCommittedCallback;
  TextInput(int maximum_width_pixels,
            float font_height_meters,
            OnFocusChangedCallback focus_changed_callback,
            OnInputEditedCallback input_edit_callback);
  ~TextInput() override;

  void OnButtonUp(const gfx::PointF& position) override;
  void OnFocusChanged(bool focused) override;
  void OnInputEdited(const TextInputInfo& info) override;
  void OnInputCommitted(const TextInputInfo& info) override;

  void RequestFocus();
  void RequestUnfocus();
  void UpdateInput(const TextInputInfo& info);

  void SetHintText(const base::string16& text);
  void SetTextColor(SkColor color);
  void SetCursorColor(SkColor color);
  void SetHintColor(SkColor color);
  void SetTextInputDelegate(TextInputDelegate* text_input_delegate);

  void set_input_committed_callback(const OnInputCommittedCallback& callback) {
    input_commit_callback_ = callback;
  }

  bool OnBeginFrame(const base::TimeTicks& time,
                    const gfx::Vector3dF& look_at) final;
  void OnSetSize(const gfx::SizeF& size) final;
  void OnSetName() final;

  Text* get_hint_element() { return hint_element_; }
  Text* get_text_element() { return text_element_; }
  Rect* get_cursor_element() { return cursor_element_; }

 private:
  void LayOutChildren() final;
  bool SetCursorBlinkState(const base::TimeTicks& time);

  OnFocusChangedCallback focus_changed_callback_;
  OnInputEditedCallback input_edit_callback_;
  OnInputEditedCallback input_commit_callback_;
  TextInputDelegate* delegate_ = nullptr;
  TextInputInfo text_info_;
  bool focused_ = false;
  bool cursor_visible_ = false;

  Text* hint_element_ = nullptr;
  Text* text_element_ = nullptr;
  Rect* cursor_element_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TextInput);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_INPUT_H_
