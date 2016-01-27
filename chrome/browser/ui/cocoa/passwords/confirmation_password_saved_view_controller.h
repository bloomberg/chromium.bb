// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_CONFIRMATION_PASSWORD_SAVED_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_CONFIRMATION_PASSWORD_SAVED_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"

@class HyperlinkTextView;

// Manages the view that confirms that the generated password was saved.
@interface ConfirmationPasswordSavedViewController
    : BasePasswordsContentViewController<NSTextViewDelegate> {
 @private
  base::scoped_nsobject<HyperlinkTextView> confirmationText_;
  base::scoped_nsobject<NSButton> okButton_;
}
@end

@interface ConfirmationPasswordSavedViewController (Testing)
@property(readonly) HyperlinkTextView* confirmationText;
@property(readonly) NSButton* okButton;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_CONFIRMATION_PASSWORD_SAVED_VIEW_CONTROLLER_H_
