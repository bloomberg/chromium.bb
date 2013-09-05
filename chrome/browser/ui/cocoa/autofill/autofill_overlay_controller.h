// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_OVERLAY_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

namespace autofill {
class AutofillDialogViewDelegate;
struct DialogOverlayState;
}  // autofill

namespace ui {
class Animation;
class MultiAnimation;
}  // ui

class AnimationDelegateBridge;
class OverlayTimerBridge;

@class AutofillMessageStackView;

// Protocol that allows Cocoa objects to act as a delegate for a ui::Animation.
@protocol AnimationDelegate
- (void)animationProgressed:(const ui::Animation*)animation;
- (void)animationEnded:(const ui::Animation*)animation;
@end


@interface AutofillOverlayController :
    NSViewController<AnimationDelegate, AutofillLayout> {
 @private
  // |childView_| contains all overlay UI elements. This is used to fade out
  // UI elements first, before making the main view transparent to fade out the
  // overlay shield.
  base::scoped_nsobject<NSView> childView_;
  base::scoped_nsobject<NSImageView> imageView_;
  base::scoped_nsobject<AutofillMessageStackView> messageStackView_;

  scoped_ptr<ui::MultiAnimation> fadeOutAnimation_;  // Animation rules.
  scoped_ptr<AnimationDelegateBridge> animationDelegate_;

  // Timer to control refresh rate of the overlay's state.
  scoped_ptr<OverlayTimerBridge> refreshTimer_;

  autofill::AutofillDialogViewDelegate* delegate_;  // not owned, owns dialog.
}

// Designated initializer.
- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate;

// Updates the state from the dialog controller.
- (void)updateState;

// Sets a specific state for the overlay.
- (void)setState:(const autofill::DialogOverlayState&)state;

// Start the animation sequence. At its end, the dialog will hide itself.
- (void)beginFadeOut;

// Get the preferred view height for a given width.
- (CGFloat)heightForWidth:(int)width;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_OVERLAY_CONTROLLER_H_

