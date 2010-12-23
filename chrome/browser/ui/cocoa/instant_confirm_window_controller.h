// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INSTANT_CONFIRM_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INSTANT_CONFIRM_WINDOW_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"

class Profile;

// This controller manages a dialog that informs the user about Instant search.
// The recommended API is to not use this class directly, but instead to use
// the functions in //chrome/browser/instant/instant_confirm_dialog.h:
//   void ShowInstantConfirmDialog[IfNecessary](gfx::NativeWindow* parent, ...)
// Which will attach the window to |parent| as a sheet.
@interface InstantConfirmWindowController : NSWindowController<NSWindowDelegate>
{
 @private
  // The long description about Instant that needs to be sized-to-fit.
  IBOutlet NSTextField* description_;

  Profile* profile_;  // weak
}

// Designated initializer. The controller will release itself on window close.
- (id)initWithProfile:(Profile*)profile;

// Action for the "Learn more" link.
- (IBAction)learnMore:(id)sender;

// The user has opted in to Instant. This enables the Instant preference.
- (IBAction)ok:(id)sender;

// Closes the sheet without altering the preference value.
- (IBAction)cancel:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INSTANT_CONFIRM_WINDOW_CONTROLLER_H_
