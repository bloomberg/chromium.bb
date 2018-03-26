// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_PROMPT_H_
#define CHROME_BROWSER_VR_ELEMENTS_PROMPT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class PromptTexture;
struct ButtonColors;

class Prompt : public TexturedElement {
 public:
  enum Button { NONE, PRIMARY, SECONDARY };

  typedef typename base::RepeatingCallback<void(Button, UiUnsupportedMode)>
      PromptCallback;

  Prompt(int preferred_width,
         int content_message_id,
         const gfx::VectorIcon& icon,
         int primary_button_message_id,
         int secondary_button_message_id,
         const PromptCallback& result_callback);
  ~Prompt() override;

  void set_reason(UiUnsupportedMode reason) { reason_ = reason; }

  void OnHoverEnter(const gfx::PointF& position) override;
  void OnHoverLeave() override;
  void OnMove(const gfx::PointF& position) override;
  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;

  void SetContentMessageId(int message_id);
  void SetIconColor(SkColor color);
  void SetPrimaryButtonColors(const ButtonColors& colors);
  void SetSecondaryButtonColors(const ButtonColors& colors);

  void Cancel();

  void SetTextureForTesting(std::unique_ptr<PromptTexture> texture);
  void ClickPrimaryButtonForTesting();
  void ClickSecondaryButtonForTesting();

 protected:
  UiTexture* GetTexture() const override;

 private:
  void OnStateUpdated(const gfx::PointF& position);

  bool primary_down_ = false;
  bool secondary_down_ = false;
  UiUnsupportedMode reason_ = UiUnsupportedMode::kCount;

  std::unique_ptr<PromptTexture> texture_;

  PromptCallback result_callback_;

  DISALLOW_COPY_AND_ASSIGN(Prompt);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_PROMPT_H_
