// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_
#define CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_

#include <memory>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/input/single_scrollbar_animation_controller_thinning.h"

namespace cc {

// This class fade in scrollbars when scroll and fade out after an idle delay.
// The fade animations works on both scrollbars and is controlled in this class.

// TODO(chaopeng) clean up the inheritance hierarchy after
// ScrollbarAnimationControllerLinearFade merge into
// ScrollbarAnimationControllerThinning so we can remove the empty overrides and
// NOTREACHED() checks.
class CC_EXPORT ScrollbarAnimationControllerThinning
    : public ScrollbarAnimationController {
 public:
  static std::unique_ptr<ScrollbarAnimationControllerThinning> Create(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta delay_before_starting,
      base::TimeDelta resize_delay_before_starting,
      base::TimeDelta fade_duration,
      base::TimeDelta thinning_duration);

  ~ScrollbarAnimationControllerThinning() override;

  void DidScrollUpdate(bool on_resize) override;
  void DidScrollEnd() override;
  bool ScrollbarsHidden() const override;

 protected:
  bool NeedThinningAnimation() const override;

 private:
  ScrollbarAnimationControllerThinning(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta delay_before_starting,
      base::TimeDelta resize_delay_before_starting,
      base::TimeDelta fade_duration,
      base::TimeDelta thinning_duration);

  void ApplyOpacityToScrollbars(float opacity) override;
  void RunAnimationFrame(float progress) override;
  const base::TimeDelta& Duration() override;

  float opacity_;
  base::TimeDelta fade_duration_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarAnimationControllerThinning);
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_
