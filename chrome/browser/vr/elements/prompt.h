// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_PROMPT_H_
#define CHROME_BROWSER_VR_ELEMENTS_PROMPT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/exit_prompt.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class Prompt : public ExitPrompt {
 public:
  Prompt(int preferred_width,
         int content_message_id,
         const gfx::VectorIcon& icon,
         int primary_button_message_id,
         int secondary_button_message_id,
         const ExitPromptCallback& result_callback);
  ~Prompt() override;

  void SetIconColor(SkColor color);

 private:
  void OnStateUpdated(const gfx::PointF& position);

  DISALLOW_COPY_AND_ASSIGN(Prompt);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_PROMPT_H_
