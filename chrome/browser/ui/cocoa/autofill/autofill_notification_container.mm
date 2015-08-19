// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_notification_container.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_notification_controller.h"

@implementation AutofillNotificationContainer

- (id)initWithDelegate:(autofill::AutofillDialogViewDelegate*)delegate {
  if (self = [super init]) {
    delegate_ = delegate;
    [self setView:[[[NSView alloc] initWithFrame:NSZeroRect] autorelease]];
  }
  return self;
}

// Just here to satisfy the protocol - not actually invoked.
- (NSSize)preferredSize {
  NOTREACHED();
  return NSZeroSize;
}

- (NSSize)preferredSizeForWidth:(CGFloat)width {
  NSSize preferredSize = NSMakeSize(width, 0);

  if ([notificationControllers_ count] == 0)
    return preferredSize;

  // A bit of padding above the arrow.
  preferredSize.height += autofill::kDetailVerticalPadding;

  for (AutofillNotificationController* controller in
       notificationControllers_.get()) {
    preferredSize.height += [controller preferredSizeForWidth:width].height;
  }

  preferredSize.height += autofill::kDetailVerticalPadding;

  return preferredSize;
}

- (void)performLayout {
  if ([notificationControllers_ count] == 0)
    return;

  NSRect remaining = [[self view] bounds];
  remaining.origin.y += autofill::kDetailVerticalPadding;
  remaining.size.height -= 2 * autofill::kDetailVerticalPadding;

  for (AutofillNotificationController* controller in
       notificationControllers_.get()) {
    NSRect viewRect;
    NSSize size = [controller preferredSizeForWidth:NSWidth(remaining)];
    NSDivideRect(remaining, &viewRect, &remaining, size.height, NSMaxYEdge);
    [[controller view ] setFrame:viewRect];
    [controller performLayout];
  }
  DCHECK_EQ(0, NSHeight(remaining));
}

- (void)setNotifications:(const autofill::DialogNotifications&)notifications {
  notificationControllers_.reset([[NSMutableArray alloc] init]);
  [[self view] setSubviews:@[]];

  for (size_t i = 0; i < notifications.size(); ++i) {
    // Create basic notification view.
    const autofill::DialogNotification& notification = notifications[i];
    base::scoped_nsobject<AutofillNotificationController>
        notificationController([[AutofillNotificationController alloc]
                                    initWithNotification:&notification
                                                delegate:delegate_]);

    [notificationControllers_ addObject:notificationController];
    [[self view] addSubview:[notificationController view]];
  }
}

@end
