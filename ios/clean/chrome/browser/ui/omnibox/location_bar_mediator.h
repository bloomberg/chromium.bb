// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_MEDIATOR_H_

#import <Foundation/Foundation.h>
#include <memory>

#import "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"

class LocationBarController;
class WebStateList;

// LocationBarMediator listens for notifications from various models and updates
// the UI accordingly.  Unlike other mediators in the new architecture,
// LocationBarMediator is constrained to operate within the confines of the
// cross-platform omnibox component.  Specifically, LocationBarMediator does not
// have a |consumer| property, but rather it updates the UI through methods on
// LocationBarController and OmniboxViewIOS.
@interface LocationBarMediator : NSObject<LocationBarDelegate>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets the LocationBarController that this mediator should use when updating
// UI.  Unlike other mediators in the new architecture, LocationBarMediator
// updates the UI through this LocationBarController object rather than through
// a consumer.
- (void)setLocationBar:(std::unique_ptr<LocationBarController>)locationBar;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_MEDIATOR_H_
