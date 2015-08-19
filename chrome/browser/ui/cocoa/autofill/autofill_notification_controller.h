// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_NOTIFICATION_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"
#include "url/gurl.h"

@class AutofillTooltipController;
@class HyperlinkTextView;

namespace autofill {
class AutofillDialogViewDelegate;
}

// Contains a single notification for requestAutocomplete dialog.
@interface AutofillNotificationController :
    NSViewController<AutofillLayout, NSTextViewDelegate> {
 @private
  // Text view for label.
  base::scoped_nsobject<HyperlinkTextView> textview_;

  // Optional tooltip icon.
  base::scoped_nsobject<AutofillTooltipController> tooltipController_;

  // Optional link target.
  GURL linkURL_;

  // Notification type.
  autofill::DialogNotification::Type notificationType_;

  // Main delegate for the dialog. Weak, owns dialog.
  autofill::AutofillDialogViewDelegate* delegate_;
}

@property(nonatomic, readonly) NSTextView* textview;
@property(nonatomic, readonly) NSView* tooltipView;

// Designated initializer. Initializes the controller as specified by
// |notification|.
- (id)initWithNotification:(const autofill::DialogNotification*)notification
                  delegate:(autofill::AutofillDialogViewDelegate*)delegate;

// Compute preferred size for given width.
- (NSSize)preferredSizeForWidth:(CGFloat)width;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_NOTIFICATION_CONTROLLER_H_
