// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_SAVE_PENDING_PASSWORD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_SAVE_PENDING_PASSWORD_VIEW_CONTROLLER_H_

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"
#import "ui/base/cocoa/touch_bar_forward_declarations.h"

// Manages the view that offers to save the user's password.
@interface SavePendingPasswordViewController
    : PendingPasswordViewController<NSTextViewDelegate, NSTouchBarDelegate> {
 @private
  base::scoped_nsobject<NSButton> saveButton_;
  base::scoped_nsobject<NSButton> neverButton_;
  base::scoped_nsobject<NSButton> editButton_;
  // Container for credentials row. Holds a PendingPasswordItemView instance.
  base::scoped_nsobject<NSView> passwordItemContainer_;
  BOOL editMode_;
}

- (NSView*)createPasswordView;
- (NSArray*)createButtonsAndAddThemToView:(NSView*)view;

// Overridden to customize the touch bar.
- (NSTouchBar*)makeTouchBar API_AVAILABLE(macos(10.12.2));
@end

@interface SavePendingPasswordViewController (Testing)
@property(readonly) NSButton* saveButton;
@property(readonly) NSButton* neverButton;
@property(readonly) NSButton* editButton;
@property(readonly) NSView* passwordItemContainer;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_SAVE_PENDING_PASSWORD_VIEW_CONTROLLER_H_
