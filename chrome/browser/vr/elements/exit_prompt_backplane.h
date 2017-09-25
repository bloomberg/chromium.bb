// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_BACKPLANE_H_
#define CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_BACKPLANE_H_

#include "base/callback.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"

namespace vr {

// An invisible but hittable plane behind the exit prompt, to keep the reticle
// roughly planar with the prompt when its near the prompt.
class ExitPromptBackplane : public InvisibleHitTarget {
 public:
  explicit ExitPromptBackplane(const base::Callback<void()>& click_callback);
  ~ExitPromptBackplane() override;

  void OnButtonUp(const gfx::PointF& position) override;

 private:
  base::Callback<void()> click_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExitPromptBackplane);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_BACKPLANE_H_
