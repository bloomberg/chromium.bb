// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_INSTANT_OPT_IN_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_INSTANT_OPT_IN_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

class Profile;

// This delegate receives callbacks from the InstantOptInController when the OK
// and Cancel buttons are pushed.
class InstantOptInControllerDelegate {
 public:
  virtual void UserPressedOptIn(bool opt_in) = 0;

 protected:
  virtual ~InstantOptInControllerDelegate() {}
};

// Manages an instant opt-in view, which is part of the omnibox popup.
@interface InstantOptInController : NSViewController {
 @private
  InstantOptInControllerDelegate* delegate_;  // weak

  // Needed in order to localize text and resize to fit.
  IBOutlet NSButton* okButton_;
  IBOutlet NSButton* cancelButton_;
  IBOutlet NSTextField* label_;
}

// Designated initializer.
- (id)initWithDelegate:(InstantOptInControllerDelegate*)delegate;

// Button actions.
- (IBAction)ok:(id)sender;
- (IBAction)cancel:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_INSTANT_OPT_IN_CONTROLLER_H_
