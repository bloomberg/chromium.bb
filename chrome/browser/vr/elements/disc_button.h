// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_DISC_BUTTON_H_
#define CHROME_BROWSER_VR_ELEMENTS_DISC_BUTTON_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/button.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class VectorIcon;

// A disc button has a circle as the background and a vector icon as the
// foreground.  When hovered, background and foreground both move forward on Z
// axis.  This matches the Daydream disk-style button.
class DiscButton : public Button {
 public:
  DiscButton(base::RepeatingCallback<void()> click_handler,
             const gfx::VectorIcon& icon,
             AudioDelegate* audio_delegate);
  ~DiscButton() override;

  VectorIcon* foreground() const { return foreground_; }

  void set_icon_scale_factor(float factor) { icon_scale_factor_ = factor; }

 private:
  void OnStateUpdated() override;
  void OnSetDrawPhase() override;
  void OnSetName() override;
  void OnSetCornerRadii(const CornerRadii& radii) override;
  void NotifyClientSizeAnimated(const gfx::SizeF& size,
                                int target_property_id,
                                cc::KeyframeModel* keyframe_model) override;

  // This button will automatically scale down the given icon to fit the button.
  // This value is used to determine the amount of scaling and can be set
  // externally to create a smaller or larger icon.
  float icon_scale_factor_;
  VectorIcon* foreground_;
  DISALLOW_COPY_AND_ASSIGN(DiscButton);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_DISC_BUTTON_H_
