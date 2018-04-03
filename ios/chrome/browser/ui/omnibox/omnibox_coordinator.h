// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_

#import <UIKit/UIKit.h>

namespace ios {
class ChromeBrowserState;
}
class WebOmniboxEditController;
@class CommandDispatcher;
@class OmniboxPopupCoordinator;
@class OmniboxTextFieldIOS;
@protocol OmniboxPopupPositioner;

// The coordinator for the omnibox.
@interface OmniboxCoordinator : NSObject
// The textfield coordinated by this object. This has to be created elsewhere
// and passed into this object before it's started.
@property(nonatomic, strong) OmniboxTextFieldIOS* textField;
// The edit controller interfacing the |textField| and the omnibox components
// code. Needs to be set before the coordinator is started.
@property(nonatomic, assign) WebOmniboxEditController* editController;
// Weak reference to ChromeBrowserState;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// The dispatcher for this view controller.
@property(nonatomic, weak) CommandDispatcher* dispatcher;

// Start this coordinator. When it starts, it expects to have |textField| and
// |editController|.
- (void)start;
// Stop this coordinator.
- (void)stop;
// Update the contents and the styling of the omnibox.
- (void)updateOmniboxState;
// Marks the next omnibox focus event source as the fakebox.
- (void)setNextFocusSourceAsSearchButton;
// Use this method to resign |textField| as the first responder.
- (void)endEditing;
// Creates a child popup coordinator. The popup coordinator is linked to the
// |textField| through components code.
- (OmniboxPopupCoordinator*)createPopupCoordinator:
    (id<OmniboxPopupPositioner>)positioner;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_
