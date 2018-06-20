// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_LEGACY_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_LEGACY_PRESENTER_H_

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_generic_presenter.h"

// The pre-UI Refresh implementation of the popup presenter.
@interface OmniboxPopupLegacyPresenter : NSObject<OmniboxPopupGenericPresenter>

- (instancetype)initWithPopupPositioner:(id<OmniboxPopupPositioner>)positioner
                    popupViewController:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_LEGACY_PRESENTER_H_
