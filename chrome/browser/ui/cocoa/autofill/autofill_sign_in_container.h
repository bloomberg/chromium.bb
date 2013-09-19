// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SIGN_IN_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SIGN_IN_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"

namespace autofill {
class AutofillDialogCocoa;
class AutofillDialogSignInDelegate;
}

namespace content {
class WebContents;
class NavigationController;
}

// Controls the sign-in dialog of the AutofillDialog.
@interface AutofillSignInContainer : NSViewController {
 @private
  autofill::AutofillDialogCocoa* dialog_;  // Not owned.
  scoped_ptr<content::WebContents> webContents_;
  scoped_ptr<autofill::AutofillDialogSignInDelegate> signInDelegate_;
}

- (id)initWithDialog:(autofill::AutofillDialogCocoa*)dialog;
- (void)loadSignInPage;
- (content::NavigationController*)navigationController;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SIGN_IN_CONTAINER_H_
