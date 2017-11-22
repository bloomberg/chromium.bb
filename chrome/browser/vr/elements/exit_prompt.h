// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_H_
#define CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"

namespace vr {

class ExitPromptTexture;
struct ButtonColors;

class ExitPrompt : public TexturedElement {
 public:
  typedef typename base::Callback<void(UiUnsupportedMode)> Callback;
  ExitPrompt(int preferred_width,
             const Callback& primary_button_callback,
             const Callback& secondary_buttton_callback);
  ~ExitPrompt() override;

  void SetContentMessageId(int message_id);

  void SetTextureForTesting(ExitPromptTexture* texture);

  void set_reason(UiUnsupportedMode reason) { reason_ = reason; }

  void OnHoverEnter(const gfx::PointF& position) override;
  void OnHoverLeave() override;
  void OnMove(const gfx::PointF& position) override;
  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;

  void SetPrimaryButtonColors(const ButtonColors& colors);
  void SetSecondaryButtonColors(const ButtonColors& colors);

  void ClickPrimaryButtonForTesting();
  void ClickSecondaryButtonForTesting();

 private:
  UiTexture* GetTexture() const override;

  void OnStateUpdated(const gfx::PointF& position);

  bool primary_down_ = false;
  bool secondary_down_ = false;
  UiUnsupportedMode reason_ = UiUnsupportedMode::kCount;

  std::unique_ptr<ExitPromptTexture> texture_;

  Callback primary_button_callback_;
  Callback secondary_button_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExitPrompt);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_H_
