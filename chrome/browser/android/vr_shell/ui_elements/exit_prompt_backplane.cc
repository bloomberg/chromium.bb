// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/exit_prompt_backplane.h"

#include "base/memory/ptr_util.h"

namespace vr_shell {

ExitPromptBackplane::ExitPromptBackplane(
    const base::Callback<void()>& click_callback)
    : click_callback_(click_callback) {}

ExitPromptBackplane::~ExitPromptBackplane() = default;

void ExitPromptBackplane::OnButtonUp(const gfx::PointF& position) {
  // We always run the callback. That is, if the user clicks the controller
  // button on the backplane and releases it on another element in front of the
  // backplane, this callback will still be called. Elements are input locked
  // on button down and it's probably not worth special-casing the backplane.
  click_callback_.Run();
}

}  // namespace vr_shell
