// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INFOBAR_CONTAINER_DELEGATE_H_
#define CHROME_BROWSER_UI_INFOBAR_CONTAINER_DELEGATE_H_

#include <stddef.h>

#include "base/macros.h"
#include "components/infobars/core/infobar_container.h"

class InfoBarContainerDelegate : public infobars::InfoBarContainer::Delegate {
 public:
  static const int kDefaultBarTargetHeight;
  static const int kSeparatorLineHeight;
  static const int kDefaultArrowTargetHeight;
  static const int kMaximumArrowTargetHeight;
  static const int kDefaultArrowTargetHalfWidth;
  static const int kMaximumArrowTargetHalfWidth;

  InfoBarContainerDelegate();
  ~InfoBarContainerDelegate() override;

  // Called when the distance between what the top infobar's "unspoofable" arrow
  // would point to and the top infobar itself changes.  Sets the maximum height
  // of the top arrow to the new |height| and asks |container| to update its
  // infobars accordingly.  This enables the top infobar to show a longer arrow
  // (e.g. because of a visible bookmark bar) or shorter (e.g. due to being in a
  // popup window) if desired.
  void SetMaxTopArrowHeight(int height, infobars::InfoBarContainer* container);

  // infobars::InfoBarContainer::Delegate:
  int ArrowTargetHeightForInfoBar(
      size_t index,
      const gfx::SlideAnimation& animation) const override;
  void ComputeInfoBarElementSizes(const gfx::SlideAnimation& animation,
                                  int arrow_target_height,
                                  int bar_target_height,
                                  int* arrow_height,
                                  int* arrow_half_width,
                                  int* bar_height) const override;

 private:
  int top_arrow_target_height_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerDelegate);
};

#endif  // CHROME_BROWSER_UI_INFOBAR_CONTAINER_DELEGATE_H_
