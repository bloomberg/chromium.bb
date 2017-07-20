// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_H_
#define CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/textured_element.h"

namespace vr {

class ExitPromptTexture;

class ExitPrompt : public TexturedElement {
 public:
  ExitPrompt(int preferred_width,
             const base::Callback<void()>& primary_button_callback,
             const base::Callback<void()>& secondary_buttton_callback);
  ~ExitPrompt() override;

  void SetContentMessageId(int message_id);

  void SetTextureForTesting(ExitPromptTexture* texture);

  void OnHoverEnter(const gfx::PointF& position) override;
  void OnHoverLeave() override;
  void OnMove(const gfx::PointF& position) override;
  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;

 private:
  UiTexture* GetTexture() const override;

  void OnStateUpdated(const gfx::PointF& position);

  bool primary_down_ = false;
  bool secondary_down_ = false;

  std::unique_ptr<ExitPromptTexture> texture_;

  base::Callback<void()> primary_button_callback_;
  base::Callback<void()> secondary_buttton_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExitPrompt);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_H_
