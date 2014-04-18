// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_CONTAINER_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_CONTAINER_COCOA_H_

#include "components/infobars/core/infobar_container.h"

@class InfoBarContainerController;

// The cocoa specific implementation of InfoBarContainer. This mostly serves as
// a bridge for InfoBarContainerController.
class InfoBarContainerCocoa : public infobars::InfoBarContainer,
                              public infobars::InfoBarContainer::Delegate {
 public:
  explicit InfoBarContainerCocoa(InfoBarContainerController* controller);
  virtual ~InfoBarContainerCocoa();

 private:
  // InfoBarContainer:
  virtual void PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                          size_t position) OVERRIDE;
  virtual void PlatformSpecificRemoveInfoBar(
      infobars::InfoBar* infobar) OVERRIDE;

  // InfoBarContainer::Delegate:
  virtual SkColor GetInfoBarSeparatorColor() const OVERRIDE;
  virtual void InfoBarContainerStateChanged(bool is_animating) OVERRIDE;
  virtual bool DrawInfoBarArrows(int* x) const OVERRIDE;

  InfoBarContainerController* controller_;  // weak, owns us.

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_INFOBAR_CONTAINER_COCOA_H_
