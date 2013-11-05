// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_NOTIFICATION_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_NOTIFICATION_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

@class AutofillArrowView;

namespace autofill {
  class DialogNotification;
  typedef std::vector<DialogNotification> DialogNotifications;
  class AutofillDialogViewDelegate;
}

// Container for all notifications shown in requestAutocomplete dialog.
@interface AutofillNotificationContainer : NSViewController<AutofillLayout> {
 @private
  // Array of all AutofillNotificationControllers.
  base::scoped_nsobject<NSMutableArray> notificationControllers_;

  // View that the arrow is anchored to. Weak.
  NSView* anchorView_;

  // Main delegate for the dialog. Weak, owns dialog.
  autofill::AutofillDialogViewDelegate* delegate_;
}

// Designated initializer.
- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate;

// Computes the views preferred size given a fixed width.
- (NSSize)preferredSizeForWidth:(CGFloat)width;

// Sets the notification contents.
- (void)setNotifications:(const autofill::DialogNotifications&) notifications;

// Sets a view that the arrow is anchored to - center of arrow will be aligned
// with center of anchorView. (horizontally only).
- (void)setAnchorView:(NSView*)anchorView;

@end

#endif // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_NOTIFICATION_CONTAINER_H_
