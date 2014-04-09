// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_SIGNIN_CONFIRMATION_VIEW_CONTROLLER_
#define CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_SIGNIN_CONFIRMATION_VIEW_CONTROLLER_

#import <Cocoa/Cocoa.h>
#include <string>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"

@class HyperlinkTextView;
class Browser;
class Profile;

namespace ui {
class ProfileSigninConfirmationDelegate;
}

@interface ProfileSigninConfirmationViewController
    : NSViewController<NSTextViewDelegate> {
 @private
  // The browser object for the sign-in tab.
  Browser* browser_;

  // The GAIA username being signed in.
  std::string username_;

  // Indicates whether the user should be given the option to
  // create a new profile before completing sign-in.
  bool offerProfileCreation_;

  // Dialog button callbacks.
  ui::ProfileSigninConfirmationDelegate* delegate_;
  base::Closure closeDialogCallback_;

  // UI elements.
  base::scoped_nsobject<NSBox> promptBox_;
  base::scoped_nsobject<NSButton> closeButton_;
  base::scoped_nsobject<NSTextField> titleField_;
  base::scoped_nsobject<NSTextField> promptField_;
  base::scoped_nsobject<NSTextView> explanationField_;
  base::scoped_nsobject<ConstrainedWindowButton> createProfileButton_;
  base::scoped_nsobject<ConstrainedWindowButton> cancelButton_;
  base::scoped_nsobject<ConstrainedWindowButton> okButton_;
}

- (id)initWithBrowser:(Browser*)browser
             username:(const std::string&)username
             delegate:(ui::ProfileSigninConfirmationDelegate*)delegate
  closeDialogCallback:(const base::Closure&)closeDialogCallback
 offerProfileCreation:(bool)offer;
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;
- (IBAction)close:(id)sender;
- (IBAction)createProfile:(id)sender;

@end

@interface ProfileSigninConfirmationViewController (TestingAPI)

@property(readonly, nonatomic) ui::ProfileSigninConfirmationDelegate* delegate;
@property(readonly, nonatomic) NSButton* createProfileButton;
@property(readonly, nonatomic) NSTextView* explanationField;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_PROFILE_SIGNIN_CONFIRMATION_VIEW_CONTROLLER_
