// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_DELEGATE_IOS_H_

#import <Foundation/Foundation.h>

#include "components/infobars/core/infobar_container.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class SlideAnimation;
}

@protocol InfobarContainerStateDelegate;

// iOS implementation of InfoBarContainer::Delegate. Most of the method
// implementations are no-ops, since the iOS UI should not be driven or managed
// by the cross-platform infobar code.
class InfoBarContainerDelegateIOS
    : public infobars::InfoBarContainer::Delegate {
 public:
  explicit InfoBarContainerDelegateIOS(
      id<InfobarContainerStateDelegate> delegate)
      : delegate_(delegate) {}

  ~InfoBarContainerDelegateIOS() override {}

  // This method always returns 0 on iOS.
  int ArrowTargetHeightForInfoBar(
      size_t index,
      const gfx::SlideAnimation& animation) const override;

  // This always sets |arrow_height| and |arrow_half_width| to zero, and
  // interpolates |bar_height| between zero and |bar_target_height| according
  // to |animation|.
  // It is a programming error to pass -1 in as the |bar_target_height| on iOS,
  // signalling that the default infobar height should be used (there is no such
  // default on iOS).
  void ComputeInfoBarElementSizes(const gfx::SlideAnimation& animation,
                                  int arrow_target_height,
                                  int bar_target_height,
                                  int* arrow_height,
                                  int* arrow_half_width,
                                  int* bar_height) const override;

  // Informs |delegate_| that the container state has changed.
  void InfoBarContainerStateChanged(bool is_animating) override;

  // This method always returns false on iOS.
  bool DrawInfoBarArrows(int* x) const override;

 private:
  __weak id<InfobarContainerStateDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_DELEGATE_IOS_H_
