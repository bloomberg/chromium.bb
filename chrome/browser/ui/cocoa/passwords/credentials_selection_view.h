// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_CREDENTIALS_SELECTION_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_CREDENTIALS_SELECTION_VIEW_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"

class ManagePasswordsBubbleModel;
namespace autofill {
struct PasswordForm;
}  // namespace autofill

// Shows a combobox for choosing username and obscured password in a single row.
@interface CredentialsSelectionView : NSView {
 @private
  ManagePasswordsBubbleModel* model_;  // weak
  base::scoped_nsobject<NSPopUpButton> usernamePopUpButton_;
  base::scoped_nsobject<NSSecureTextField> passwordField_;
}
- (id)initWithModel:(ManagePasswordsBubbleModel*)model;
- (const autofill::PasswordForm*)getSelectedCredentials;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_CREDENTIALS_SELECTION_VIEW_H_
