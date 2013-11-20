// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_LOADING_SHIELD_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_LOADING_SHIELD_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

class AutofillLoadingAnimationBridge;

namespace autofill {
class AutofillDialogViewDelegate;
class LoadingAnimation;
}

// Controller for the "Loading..." shield view of the Autofill dialog.
@interface AutofillLoadingShieldController : NSViewController<AutofillLayout> {
 @private
  // The main label for the shield.
  base::scoped_nsobject<NSTextField> message_;

  // The animate dots that follow the |message_|.
  base::scoped_nsobject<NSArray> dots_;

  // C++ bridge class for animating the dots.
  scoped_ptr<AutofillLoadingAnimationBridge> animationDriver_;

  autofill::AutofillDialogViewDelegate* delegate_;  // not owned, owns dialog.
}

// Designated initializer.
- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate;

// Updates the layout of the loading shield based on the |delegate_|'s state.
- (void)update;

// Updates the positions of the dots to match the current frame of the
// |animation|.
- (void)relayoutDotsForSteppedAnimation:
    (const autofill::LoadingAnimation&)animation;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_LOADING_SHIELD_CONTROLLER_H_
