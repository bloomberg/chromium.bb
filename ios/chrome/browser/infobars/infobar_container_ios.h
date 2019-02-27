// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_IOS_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_IOS_H_

#include "components/infobars/core/infobar_container.h"

#include "base/macros.h"

@protocol InfobarContainerConsumer;

// IOS infobar container specialization, managing infobars visibility and
// presentation via the InfobarContainerConsumer protocol.|legacyConsumer| is
// used to support the legacy InfobarPresentation concurrently with the new one
// that uses |consumer|.
class InfoBarContainerIOS : public infobars::InfoBarContainer {
 public:
  InfoBarContainerIOS(id<InfobarContainerConsumer> consumer,
                      id<InfobarContainerConsumer> legacyConsumer);
  ~InfoBarContainerIOS() override;

 protected:
  void PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                  size_t position) override;
  void PlatformSpecificRemoveInfoBar(infobars::InfoBar* infobar) override;
  void PlatformSpecificInfoBarStateChanged(bool is_animating) override;

 private:
  id<InfobarContainerConsumer> consumer_;
  id<InfobarContainerConsumer> legacyConsumer_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerIOS);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_IOS_H_
