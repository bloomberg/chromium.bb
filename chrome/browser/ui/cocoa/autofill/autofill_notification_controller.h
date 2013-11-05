// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_NOTIFICATION_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

namespace autofill {
class DialogNotification;
}

// Contains a single notification for requestAutocomplete dialog.
@interface AutofillNotificationController : NSViewController<AutofillLayout> {
 @private
  // NSTextField for label.
  base::scoped_nsobject<NSTextField> textfield_;

  // Optional checkbox.
  base::scoped_nsobject<NSButton> checkbox_;

  // Optional tooltip icon.
  base::scoped_nsobject<NSImageView> tooltipIcon_;
}

@property(nonatomic, readonly) NSTextField* textfield;
@property(nonatomic, readonly) NSButton* checkbox;
@property(nonatomic, readonly) NSImageView* tooltipIcon;

// Designated initializer. Initializes the controller as specified by
// |notification|.
- (id)initWithNotification:(const autofill::DialogNotification*)notification;

// Displays arrow on top of notification if set to YES. |anchorView| determines
// the arrow position - the tip of the arrow is centered on the horizontal
// midpoint of the anchorView.
- (void)setHasArrow:(BOOL)hasArrow withAnchorView:(NSView*)anchorView;

// Indicates if the controller draws an arrow.
- (BOOL)hasArrow;

// Compute preferred size for given width.
- (NSSize)preferredSizeForWidth:(CGFloat)width;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_NOTIFICATION_CONTROLLER_H_
