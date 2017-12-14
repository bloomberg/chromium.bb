// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_

#import <UIKit/UIKit.h>

class AutocompleteResult;
class OmniboxPopupMediatorDelegate;
@protocol OmniboxPopupPositioner;

namespace ios {
class ChromeBrowserState;
}

// Coordinator for the Omnibox Popup.
@interface OmniboxPopupCoordinator : NSObject
// BrowserState.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// Positioner for the popup.
@property(nonatomic, weak) id<OmniboxPopupPositioner> positioner;
// Delegate for the popup mediator.
@property(nonatomic, assign) OmniboxPopupMediatorDelegate* mediatorDelegate;
// Whether the popup is open.
@property(nonatomic, assign, getter=isOpen) BOOL open;

- (void)start;
// Updates the popup with the |results|.
- (void)updateWithResults:(const AutocompleteResult&)results;
// Sets the text alignment of the popup content.
- (void)setTextAlignment:(NSTextAlignment)alignment;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
