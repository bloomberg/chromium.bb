// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/exit_prompt.h"

namespace vr {

class AudioPermissionPrompt : public ExitPrompt {
 public:
  AudioPermissionPrompt(int preferred_width,
                        const ExitPromptCallback& result_callback);
  ~AudioPermissionPrompt() override;

  void SetIconColor(SkColor color);

 private:
  void OnStateUpdated(const gfx::PointF& position);

  DISALLOW_COPY_AND_ASSIGN(AudioPermissionPrompt);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_H_
