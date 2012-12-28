// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/balloon_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#import "base/memory/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_options_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/hover_image_button.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#import "chrome/browser/ui/cocoa/notifications/balloon_view.h"
#include "chrome/browser/ui/cocoa/notifications/balloon_view_host_mac.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/mac/nsimage_cache.h"

namespace {

// Margin, in pixels, between the notification frame and the contents
// of the notification.
const int kTopMargin = 1;
const int kBottomMargin = 2;
const int kLeftMargin = 2;
const int kRightMargin = 2;

}  // namespace

@interface BalloonController (Private)
- (void)updateTrackingRect;
@end

@implementation BalloonController

- (id)initWithBalloon:(Balloon*)balloon {
  NSString* nibpath =
      [base::mac::FrameworkBundle() pathForResource:@"Notification"
                                             ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    balloon_ = balloon;
    [self initializeHost];
    menuModel_.reset(new NotificationOptionsMenuModel(balloon));
    menuController_.reset([[MenuController alloc] initWithModel:menuModel_.get()
                                         useWithPopUpButtonCell:NO]);
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  [optionsButton_ setDefaultImage:
      rb.GetNativeImageNamed(IDR_BALLOON_WRENCH).ToNSImage()];
  [optionsButton_ setDefaultOpacity:1.0];
  [optionsButton_ setHoverImage:
      rb.GetNativeImageNamed(IDR_BALLOON_WRENCH_H).ToNSImage()];
  [optionsButton_ setHoverOpacity:1.0];
  [optionsButton_ setPressedImage:
      rb.GetNativeImageNamed(IDR_BALLOON_WRENCH_P).ToNSImage()];
  [optionsButton_ setPressedOpacity:1.0];
  [[optionsButton_ cell] setHighlightsBy:NSNoCellMask];

  NSString* sourceLabelText = l10n_util::GetNSStringF(
      IDS_NOTIFICATION_BALLOON_SOURCE_LABEL,
      balloon_->notification().display_source());
  [originLabel_ setStringValue:sourceLabelText];

  // This condition is false in unit tests which have no RVH.
  if (htmlContents_.get()) {
    gfx::NativeView contents = htmlContents_->native_view();
    [contents setFrame:NSMakeRect(kLeftMargin, kTopMargin, 0, 0)];
    [[htmlContainer_ superview] addSubview:contents
                                positioned:NSWindowBelow
                                relativeTo:nil];
  }

  // Use the standard close button for a utility window.
  closeButton_ = [NSWindow standardWindowButton:NSWindowCloseButton
                                   forStyleMask:NSUtilityWindowMask];
  NSRect frame = [closeButton_ frame];
  [closeButton_ setFrame:NSMakeRect(6, 1, frame.size.width, frame.size.height)];
  [closeButton_ setTarget:self];
  [closeButton_ setAction:@selector(closeButtonPressed:)];
  [shelf_ addSubview:closeButton_];
  [self updateTrackingRect];

  // Set the initial position without animating (the balloon should not
  // yet be visible).
  DCHECK(![[self window] isVisible]);
  NSRect balloon_frame = NSMakeRect(balloon_->GetPosition().x(),
                                    balloon_->GetPosition().y(),
                                    [self desiredTotalWidth],
                                    [self desiredTotalHeight]);
  [[self window] setFrame:balloon_frame
                  display:NO];
}

- (void)updateTrackingRect {
  if (closeButtonTrackingTag_)
    [shelf_ removeTrackingRect:closeButtonTrackingTag_];

  closeButtonTrackingTag_ = [shelf_ addTrackingRect:[closeButton_ frame]
                                              owner:self
                                           userData:nil
                                       assumeInside:NO];
}

- (void) mouseEntered:(NSEvent*)event {
  [[closeButton_ cell] setHighlighted:YES];
}

- (void) mouseExited:(NSEvent*)event {
  [[closeButton_ cell] setHighlighted:NO];
}

- (void)closeBalloonNow:(bool)byUser {
  if (!balloon_)
    return;
  [self close];
  if (htmlContents_.get())
    htmlContents_->Shutdown();
  if (balloon_)
    balloon_->OnClose(byUser);
  balloon_ = NULL;
}

- (IBAction)optionsButtonPressed:(id)sender {
  optionMenuIsActive_ = YES;
  [NSMenu popUpContextMenu:[menuController_ menu]
                 withEvent:[NSApp currentEvent]
                   forView:optionsButton_];
  optionMenuIsActive_ = NO;
  if (delayedClose_)
    [self closeBalloonNow: false]; // always by script.
}

- (IBAction)permissionRevoked:(id)sender {
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(balloon_->profile());
  service->DenyPermission(balloon_->notification().origin_url());
}

- (IBAction)closeButtonPressed:(id)sender {
  [self closeBalloon:YES];
  [self close];
}

- (void)close {
  if (closeButtonTrackingTag_)
    [shelf_ removeTrackingRect:closeButtonTrackingTag_];

  [super close];
}

- (void)closeBalloon:(bool)byUser {
  // Keep alive while user is interacting with popup menu.
  // Otherwise the script can close the notification and underlying balloon
  // will be destroyed while user select a menu command.
  if (!byUser && optionMenuIsActive_) {
    delayedClose_ = YES;
    return;
  }
  [self closeBalloonNow: byUser];
}

- (void)updateContents {
  DCHECK(htmlContents_.get()) << "BalloonView::Update called before Show";
  if (htmlContents_->web_contents()) {
    htmlContents_->web_contents()->GetController().LoadURL(
        balloon_->notification().content_url(), content::Referrer(),
        content::PAGE_TRANSITION_LINK, std::string());
  }
}

- (void)repositionToBalloon {
  DCHECK(balloon_);
  int x = balloon_->GetPosition().x();
  int y = balloon_->GetPosition().y();
  int w = [self desiredTotalWidth];
  int h = [self desiredTotalHeight];

  [[self window] setFrame:NSMakeRect(x, y, w, h)
                  display:YES];
  if (htmlContents_.get())
    htmlContents_->UpdateActualSize(balloon_->content_size());
}

// Returns the total width the view should be to accommodate the balloon.
- (int)desiredTotalWidth {
  return (balloon_ ? balloon_->content_size().width() : 0) +
      kLeftMargin + kRightMargin;
}

// Returns the total height the view should be to accommodate the balloon.
- (int)desiredTotalHeight {
  return (balloon_ ? balloon_->content_size().height() : 0) +
      kTopMargin + kBottomMargin + [shelf_ frame].size.height;
}

// Returns the BalloonHost {
- (BalloonViewHost*) getHost {
  return htmlContents_.get();
}

// Initializes the renderer host showing the HTML contents.
- (void)initializeHost {
  htmlContents_.reset(new BalloonViewHost(balloon_));
  htmlContents_->Init();
}

// NSWindowDelegate notification.
- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

@end
