// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/popup_blocked_animation.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#import "base/nsimage_cache_mac.h"
#import "chrome/browser/cocoa/animatable_image.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"

class PopupBlockedAnimationObserver;

// A class for managing the Core Animation popup blocked animation.
@interface PopupBlockedAnimationMac : NSObject {
 @private
  // The observer for the TabContents we are drawing on.
  scoped_ptr<PopupBlockedAnimationObserver> observer_;
  AnimatableImage* animation_;
};

// Called by the Observer if the tab is hidden or closed.
- (void)closeAnimation;

@end

// A helper class to monitor tab hidden and closed notifications. If we receive
// such a notification, we stop the animation.
class PopupBlockedAnimationObserver : public NotificationObserver {
 public:
  PopupBlockedAnimationObserver(PopupBlockedAnimationMac* owner,
                                TabContents* tab_contents)
      : owner_(owner),
        tab_contents_(tab_contents) {
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_HIDDEN,
                   Source<TabContents>(tab_contents_));
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents_));
  }

  // Runs when a tab is hidden or destroyed. Let our owner know we should end
  // the animation.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    // This ends up deleting us.
    [owner_ closeAnimation];
  }

 private:
  // The object we need to inform when we get a notification. Weak.
  PopupBlockedAnimationMac* owner_;

  // The tab we are observing. Weak.
  TabContents* tab_contents_;

  // Used for registering to receive notifications and automatic clean up.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PopupBlockedAnimationObserver);
};

@implementation PopupBlockedAnimationMac

- (id)initWithTabContents:(TabContents*)tabContents {
  if ((self = [super init])) {
    NSImage* image = nsimage_cache::ImageNamed(@"popup_window_animation.pdf");
    CGFloat imageWidth = [image size].width;
    CGFloat imageHeight = [image size].height;

    NSView* tabContentsView = tabContents->GetNativeView();
    NSWindow* parentWindow = [tabContentsView window];
    if (!parentWindow) {
      // The tab is no longer frontmost.
      [self release];
      return nil;
    }

    NSRect windowFrame = [parentWindow frame];

    // Sanity check the size in case there's no room to display the animation.
    if (imageWidth >= NSWidth(windowFrame) ||
        imageHeight >= NSHeight(windowFrame)) {
      [self release];
      return nil;
    }

    // Create the animation window to be the top-right quadrant of the window.
    // The animation travels from the center of the window to the blocked
    // content section of the Omnibox. This will release itself.
    NSRect animationFrame = windowFrame;
    CGFloat dX = NSWidth(animationFrame) / 2 - imageWidth / 2;
    CGFloat dY = NSHeight(animationFrame) / 2 - imageHeight / 2;
    animationFrame.origin.x += dX;
    animationFrame.origin.y += dY;
    animationFrame.size.width -= dX;
    animationFrame.size.height -= dY;
    animation_ = [[AnimatableImage alloc] initWithImage:image
                                         animationFrame:animationFrame];
    [parentWindow addChildWindow:animation_ ordered:NSWindowAbove];

    // Start the animation from the center of the window.
    [animation_ setStartFrame:CGRectMake(0,
                                         imageHeight / 2,
                                         imageWidth,
                                         imageHeight)];

    // Set the end frame to be small (a la the actual blocked icon) and inset
    // slightly to the Omnibox. While the geometry won't align perfectly, it's
    // close enough for the user to take note of the new icon. These numbers
    // come from measuring the Omnibox without any page actions.
    CGRect endFrame = CGRectMake(animationFrame.size.width - 115,
                                 animationFrame.size.height - 50,
                                 16, 16);

    // If the location-bar can be found, ask it for better
    // coordinates.
    BrowserWindowController* bwc = [parentWindow windowController];
    if ([bwc isKindOfClass:[BrowserWindowController class]]) {
      LocationBarViewMac* lbvm = [bwc locationBarBridge];
      if (lbvm) {
        NSRect frame = lbvm->GetBlockedPopupRect();
        if (!NSIsEmptyRect(frame)) {
          // Convert up to the screen, then back down to the animation
          // window.
          NSPoint screenPoint = [parentWindow convertBaseToScreen:frame.origin];
          frame.origin = [animation_ convertScreenToBase:screenPoint];
          endFrame = NSRectToCGRect(frame);
        }
      }
    }

    [animation_ setEndFrame:endFrame];
    [animation_ setStartOpacity:1.0];
    [animation_ setEndOpacity:0.2];
    [animation_ setDuration:0.7];

    observer_.reset(new PopupBlockedAnimationObserver(self, tabContents));

    // When the window is about to close, release this object and remove the
    // animation from the parent window.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(windowWillClose:)
                   name:NSWindowWillCloseNotification
                 object:animation_];
    [animation_ startAnimation];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)closeAnimation {
  [animation_ close];
}

// When the animation window closes, release self.
- (void)windowWillClose:(NSNotification*)notification {
  DCHECK([[notification object] isEqual:animation_]);
  [[animation_ parentWindow] removeChildWindow:animation_];
  [self release];
}

@end

void PopupBlockedAnimation::Show(TabContents* tab_contents) {
  // The object will clean up itself at the end of the animation.
  [[PopupBlockedAnimationMac alloc] initWithTabContents:tab_contents];
}
