// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_notification_container.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_notification_controller.h"
#include "skia/ext/skia_utils_mac.h"

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

  // If the first notification doesn't have an arrow, reserve empty space.
  if (![[notificationControllers_ objectAtIndex:0] hasArrow])
    preferredSize.height += autofill::kArrowHeight;

  for (AutofillNotificationController* delegate in
       notificationControllers_.get())
    preferredSize.height += [delegate preferredSizeForWidth:width].height;

  return preferredSize;
}

- (void)performLayout {
  if ([notificationControllers_ count] == 0)
    return;

  NSRect remaining = [[self view] bounds];

  if (![[notificationControllers_ objectAtIndex:0] hasArrow])
    remaining.size.height -= autofill::kArrowHeight;

  for (AutofillNotificationController* delegate in
       notificationControllers_.get()) {
    NSRect viewRect;
    NSSize size = [delegate preferredSizeForWidth:NSWidth(remaining)];
    NSDivideRect(remaining, &viewRect, &remaining, size.height, NSMaxYEdge);
    [[delegate view ] setFrame:viewRect];
    [delegate performLayout];
  }
  DCHECK_EQ(0, NSHeight(remaining));
}

- (void)setNotifications:(const autofill::DialogNotifications&)notifications {
  notificationControllers_.reset([[NSMutableArray alloc] init]);
  checkboxNotification_.reset();
  [[self view] setSubviews:@[]];

  for (size_t i = 0; i < notifications.size(); ++i) {
    // Create basic notification view.
    const autofill::DialogNotification& notification = notifications[i];
    base::scoped_nsobject<AutofillNotificationController>
        notificationController([[AutofillNotificationController alloc] init]);
    [notificationController setText:
        base::SysUTF16ToNSString(notification.display_text())];
    [notificationController setTextColor:
        gfx::SkColorToCalibratedNSColor(notification.GetTextColor())];
    [notificationController setBackgroundColor:
        gfx::SkColorToCalibratedNSColor(notification.GetBackgroundColor())];

    // Add optional checkbox.
    if (notification.HasCheckbox()) {
      // No more than one notification with a checkbox.
      DCHECK(!checkboxNotification_);
      checkboxNotification_.reset(
          new autofill::DialogNotification(notification));
      [notificationController setHasCheckbox:YES];
      [[notificationController checkbox] setState:
          (notification.checked() ? NSOnState : NSOffState)];
      [[notificationController checkbox] setTarget:self];
      [[notificationController checkbox] setAction:@selector(checkboxClicked:)];
    }

    if (i == 0) {
      [notificationController setHasArrow:notification.HasArrow()
                           withAnchorView:anchorView_];
    }

    [notificationControllers_ addObject:notificationController];
    [[self view] addSubview:[notificationController view]];
  }
}

- (void)setAnchorView:(NSView*)anchorView {
  anchorView_ = anchorView;
}

- (IBAction)checkboxClicked:(id)sender {
  DCHECK(checkboxNotification_);
  BOOL isChecked = ([sender state] == NSOnState);
  delegate_->NotificationCheckboxStateChanged(checkboxNotification_->type(),
                                                isChecked);
}

@end
