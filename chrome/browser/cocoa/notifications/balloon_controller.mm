// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/notifications/balloon_controller.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#import "base/cocoa_protocols_mac.h"
#import "base/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/cocoa/menu_controller.h"
#include "chrome/browser/cocoa/notifications/balloon_view_host_mac.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification_options_menu_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Margin, in pixels, between the notification frame and the contents
// of the notification.
const int kTopMargin = 1;
const int kBottomMargin = 1;
const int kLeftMargin = 1;
const int kRightMargin = 1;

}  // namespace

@implementation BalloonController

- (id)initWithBalloon:(Balloon*)balloon {
  if ((self = [super initWithWindowNibName:@"Notification"])) {
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

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* image = rb.GetNSImageNamed(IDR_BALLOON_WRENCH);
  DCHECK(image);
  [optionsButton_ setImage:image];

  NSString* sourceLabelText = l10n_util::GetNSStringF(
      IDS_NOTIFICATION_BALLOON_SOURCE_LABEL,
      WideToUTF16(balloon_->notification().display_source()));
  [originLabel_ setStringValue:sourceLabelText];

  gfx::NativeView contents = htmlContents_->native_view();
  [contents setFrame:NSMakeRect(kLeftMargin, kTopMargin, 0, 0)];
  [[htmlContainer_ superview] addSubview:contents
                              positioned:NSWindowBelow
                              relativeTo:nil];
}

- (IBAction)optionsButtonPressed:(id)sender {
  [NSMenu popUpContextMenu:[menuController_ menu]
                 withEvent:[NSApp currentEvent]
                   forView:optionsButton_];
}

- (IBAction)permissionRevoked:(id)sender {
  DesktopNotificationService* service =
      balloon_->profile()->GetDesktopNotificationService();
  service->DenyPermission(balloon_->notification().origin_url());
}

- (IBAction)closeButtonPressed:(id)sender {
  [self closeBalloon:YES];
  [self close];
}

- (void)closeBalloon:(bool)byUser {
  DCHECK(balloon_);
  [self close];
  if (htmlContents_.get())
    htmlContents_->Shutdown();
  balloon_->OnClose(byUser);
  balloon_ = NULL;
}

- (void)updateContents {
  DCHECK(htmlContents_.get()) << "BalloonView::Update called before Show";
  if (htmlContents_->render_view_host())
    htmlContents_->render_view_host()->NavigateToURL(
        balloon_->notification().content_url());
}

- (void)repositionToBalloon {
  DCHECK(balloon_);
  int x = balloon_->GetPosition().x();
  int y = balloon_->GetPosition().y();
  int w = [self desiredTotalWidth];
  int h = [self desiredTotalHeight];

  htmlContents_->UpdateActualSize(balloon_->content_size());
  [[self window] setFrame:NSMakeRect(x, y, w, h)
                  display:YES
                  animate:YES];
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
