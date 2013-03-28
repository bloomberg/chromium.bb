// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_to_mobile_bubble_controller.h"

#include "base/mac/mac_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/chrome_to_mobile_service_factory.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/text/bytes_formatting.h"

namespace {

// The verical padding between the radio group and the send copy checkbox.
const CGFloat kVerticalPadding = 10;

// The duration of the "Sending..." progress throb animation in seconds.
const NSTimeInterval kProgressThrobDurationS = 1.2;

// The seconds to delay before automatically closing the bubble after sending.
const NSTimeInterval kCloseS = 3;

}  // namespace

ChromeToMobileBubbleNotificationBridge::ChromeToMobileBubbleNotificationBridge(
    ChromeToMobileBubbleController* controller,
    SEL selector)
    : controller_(controller),
      selector_(selector) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::NotificationService::AllSources());
}

// All observed notifications perform the same selector to close the bubble.
void ChromeToMobileBubbleNotificationBridge::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  [controller_ performSelector:selector_ withObject:controller_];
}

void ChromeToMobileBubbleNotificationBridge::SnapshotGenerated(
    const base::FilePath& path,
    int64 bytes) {
  [controller_ snapshotGenerated:path bytes:bytes];
}

void ChromeToMobileBubbleNotificationBridge::OnSendComplete(bool success) {
  [controller_ onSendComplete:success];
}

@interface ChromeToMobileBubbleController (Private)
// Construct and start the "Sending..." progress animation.
- (void)startProgressAnimation;

// Handle the "Sending..." progress animation.
- (void)animation:(NSAnimation *)animation
    didReachProgressMark:(NSAnimationProgress)progress;
@end

@implementation ChromeToMobileBubbleController

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   browser:(Browser*)browser {
  self = [super initWithWindowNibPath:@"ChromeToMobileBubble"
                         parentWindow:parentWindow
                           anchoredAt:NSZeroPoint];
  if (self) {
    browser_ = browser;
    service_ = ChromeToMobileServiceFactory::GetForProfile(browser->profile());
  }
  return self;
}

- (void)windowWillClose:(NSNotification*)notification {
  // Instruct the service to delete the snapshot file.
  service_->DeleteSnapshot(snapshotPath_);

  BrowserWindowController* controller = [BrowserWindowController
      browserWindowControllerForWindow:self.parentWindow];
  [controller chromeToMobileBubbleWindowWillClose];

  // Restore the Action Box icon when the bubble closes.
  LocationBarViewMac* locationBar = [controller locationBarBridge];
  if (locationBar)
    locationBar->ResetActionBoxIcon();

  // We caught a close so we don't need to observe further notifications.
  bridge_.reset(NULL);
  [progressAnimation_ stopAnimation];
  // Cancel any delayed requests that may still be pending (close, etc.).
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [super windowWillClose:notification];
}

// Override -[BaseBubbleController showWindow:] to set up UI elements.
- (void)showWindow:(id)sender {
  DCHECK(service_->HasMobiles());

  // Force load the NIB.
  NSWindow* window = [self window];

  const ListValue* mobiles = service_->GetMobiles();
  const DictionaryValue* mobile = NULL;
  string16 name;

  if (mobiles->GetSize() == 1) {
    // Set the single device title; it's for multiple devices by default.
    if (mobiles->GetDictionary(0, &mobile) &&
        mobile->GetString("name", &name)) {
      NSString* title = l10n_util::GetNSStringF(
          IDS_CHROME_TO_MOBILE_BUBBLE_SINGLE_TITLE, name);
      [title_ setStringValue:title];
    } else {
      NOTREACHED();
    }
  } else {
    // Initialize the mobile device radio buttons.
    [mobileRadioGroup_ renewRows:mobiles->GetSize() columns:1];
    NSArray* cellArray = [mobileRadioGroup_ cells];
    for (size_t i = 0; i < mobiles->GetSize(); ++i) {
      if (mobiles->GetDictionary(i, &mobile) &&
          mobile->GetString("name", &name)) {
        [[cellArray objectAtIndex:i] setTitle:SysUTF16ToNSString(name)];
      } else {
        NOTREACHED();
      }
    }
    [mobileRadioGroup_ setEnabled:YES];
    [mobileRadioGroup_ setHidden:NO];
    [mobileRadioGroup_ sizeToCells];

    // Adjust the window size to accommodate the radio group.
    CGFloat deltaY = [mobileRadioGroup_ frame].size.height + kVerticalPadding;
    NSRect windowFrame = [window frame];
    windowFrame.size.height += deltaY;
    [window setFrame:windowFrame display:YES animate:NO];
  }

  LocationBarViewMac* locationBar = [[BrowserWindowController
      browserWindowControllerForWindow:self.parentWindow] locationBarBridge];
  if (locationBar) {
    // Get the anchor point for the bubble in screen coordinates.
    NSPoint bubblePoint = locationBar->GetActionBoxAnchorPoint();
    bubblePoint = [self.parentWindow convertBaseToScreen:bubblePoint];
    [self.bubble setArrowLocation:info_bubble::kNoArrow];
    [self.bubble setCornerFlags:info_bubble::kRoundedBottomCorners];
    [self.bubble setAlignment:info_bubble::kAlignRightEdgeToAnchorEdge];
    [window setContentSize:self.bubble.frame.size];
    [window setContentView:self.bubble];
    [self setAnchorPoint:bubblePoint];

    // Show the lit Chrome To Mobile icon while the bubble is open.
    locationBar->SetActionBoxIcon(IDR_MOBILE_LIT);
  }

  // Initialize the checkbox to send an offline copy.
  NSString* sendCopyString =
      l10n_util::GetNSStringF(IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY,
          l10n_util::GetStringUTF16(
              IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY_GENERATING));
  [sendCopy_ setTitle:sendCopyString];
  [sendCopy_ setState:NSOffState];

  // Observe Chrome and ChromeToMobileService changes.
  bridge_.reset(new ChromeToMobileBubbleNotificationBridge(
      self, @selector(cancel:)));

  // Generate the MHTML snapshot now to report its size in the bubble.
  service_->GenerateSnapshot(browser_, bridge_->AsWeakPtr());

  [super showWindow:sender];
}

- (IBAction)learn:(id)sender {
  service_->LearnMore(browser_);
  [self close];
}

- (IBAction)send:(id)sender {
  // TODO(msw): Handle updates to the mobile list while the bubble is open.
  const ListValue* mobiles = service_->GetMobiles();
  // |mobileRadioGroup_| has a single row by defualt, so these always match.
  DCHECK_EQ(static_cast<NSInteger>(mobiles->GetSize()),
            [mobileRadioGroup_ numberOfRows]);
  // NSMatrix selectedRow is -1 by default (in the single mobile device case).
  const int selected_index = std::max<int>([mobileRadioGroup_ selectedRow], 0);

  const DictionaryValue* mobile = NULL;
  if (mobiles->GetDictionary(selected_index, &mobile)) {
    service_->SendToMobile(mobile,
        ([sendCopy_ state] == NSOnState) ? snapshotPath_ : base::FilePath(),
        browser_, bridge_->AsWeakPtr());
  } else {
    NOTREACHED();
  }

  // Update the bubble's contents to show the "Sending..." progress animation.
  [cancel_ setEnabled:NO];
  [send_ setEnabled:NO];
  [send_ setAlignment:NSNaturalTextAlignment];
  [self startProgressAnimation];
}

// By implementing this, ESC causes the window to go away.
- (IBAction)cancel:(id)sender {
  [self close];
}

- (void)snapshotGenerated:(const base::FilePath&)path
                    bytes:(int64)bytes {
  snapshotPath_ = path;
  NSString* text = nil;
  if (bytes > 0) {
    [sendCopy_ setEnabled:YES];
    text = l10n_util::GetNSStringF(IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY,
                                   ui::FormatBytes(bytes));
  } else {
    text = l10n_util::GetNSString(IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY_FAILED);
  }
  [sendCopy_ setTitle:text];
  [sendCopy_ setState:NSOffState];
}

- (void)onSendComplete:(bool)success {
  [progressAnimation_ stopAnimation];
  [send_ setAlignment:NSCenterTextAlignment];

  NSString* text = nil;
  if (success) {
    text = l10n_util::GetNSString(IDS_CHROME_TO_MOBILE_BUBBLE_SENT);
    [self performSelector:@selector(cancel:) withObject:nil afterDelay:kCloseS];
  } else {
    text = l10n_util::GetNSString(IDS_CHROME_TO_MOBILE_BUBBLE_ERROR);
    [error_ setHidden:NO];
  }
  [send_ setTitle:text];
}

- (void)startProgressAnimation {
  progressAnimation_.reset([[NSAnimation alloc]
      initWithDuration:kProgressThrobDurationS
        animationCurve:NSAnimationLinear]);
  [progressAnimation_ setAnimationBlockingMode:NSAnimationNonblocking];
  [progressAnimation_ setDelegate:self];
  [progressAnimation_ setProgressMarks:[NSArray arrayWithObjects:
      [NSNumber numberWithFloat:0.00], [NSNumber numberWithFloat:0.25],
      [NSNumber numberWithFloat:0.50], [NSNumber numberWithFloat:0.75],
      [NSNumber numberWithFloat:1.00], nil]];
  [progressAnimation_ startAnimation];
}

- (void)animation:(NSAnimation *)animation
    didReachProgressMark:(NSAnimationProgress)progress {
  DCHECK_EQ(animation, progressAnimation_.get());
  int id = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_3;
  // Show each of four messages for 1/4 of the animation.
  if (progress < 0.25) {
    id = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_0;
  } else if (progress < 0.50) {
    id = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_1;
  } else if (progress < 0.75) {
    id = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_2;
  } else if (progress >= 1.00) {
    // Restart the animation to continuously loop.
    [self startProgressAnimation];
  }
  [send_ setTitle:l10n_util::GetNSString(id)];
}

@end  // ChromeToMobileBubbleController

@implementation ChromeToMobileBubbleController (JustForTesting)

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   service:(ChromeToMobileService*)service {
  self = [super initWithWindowNibPath:@"ChromeToMobileBubble"
                         parentWindow:parentWindow
                           anchoredAt:NSZeroPoint];
  if (self) {
    browser_ = NULL;
    service_ = service;
  }
  return self;
}

- (void)setSendCopy:(bool)sendCopy {
  [sendCopy_ setState:(sendCopy ? NSOnState : NSOffState)];
}

- (ChromeToMobileBubbleNotificationBridge*)bridge {
  return bridge_.get();
}

@end  // ChromeToMobileBubbleController (JustForTesting)
