// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_OMNIBOX_POPUP_SC_OMNIBOX_POPUP_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_SHOWCASE_OMNIBOX_POPUP_SC_OMNIBOX_POPUP_CONTAINER_VIEW_CONTROLLER_H_

#include <UIKit/UIKit.h>

@class OmniboxPopupViewController;

@interface SCOmniboxPopupContainerViewController : UIViewController

@property(nonatomic, strong) OmniboxPopupViewController* popupViewController;

- (instancetype)initWithPopupViewController:
    (OmniboxPopupViewController*)popupViewController;

@end

#endif  // IOS_SHOWCASE_OMNIBOX_POPUP_SC_OMNIBOX_POPUP_CONTAINER_VIEW_CONTROLLER_H_
