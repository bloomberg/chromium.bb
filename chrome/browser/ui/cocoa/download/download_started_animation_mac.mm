// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the Mac implementation the download animation, displayed
// at the start of a download. The animation produces an arrow pointing
// downwards and animates towards the bottom of the window where the new
// download appears in the download shelf.

#include "chrome/browser/download/download_started_animation.h"

#import <QuartzCore/QuartzCore.h>

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_mac.h"
#include "chrome/common/notification_registrar.h"
#import "chrome/browser/ui/cocoa/animatable_image.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/base/resource/resource_bundle.h"

class DownloadAnimationTabObserver;

// A class for managing the Core Animation download animation.
// Should be instantiated using +startAnimationWithTabContents:.
@interface DownloadStartedAnimationMac : NSObject {
 @private
  // The observer for the TabContents we are drawing on.
  scoped_ptr<DownloadAnimationTabObserver> observer_;
  CGFloat imageWidth_;
  AnimatableImage* animation_;
};

+ (void)startAnimationWithTabContents:(TabContents*)tabContents;

// Called by the Observer if the tab is hidden or closed.
- (void)closeAnimation;

@end

// A helper class to monitor tab hidden and closed notifications. If we receive
// such a notification, we stop the animation.
class DownloadAnimationTabObserver : public NotificationObserver {
 public:
  DownloadAnimationTabObserver(DownloadStartedAnimationMac* owner,
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
  DownloadStartedAnimationMac* owner_;

  // The tab we are observing. Weak.
  TabContents* tab_contents_;

  // Used for registering to receive notifications and automatic clean up.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DownloadAnimationTabObserver);
};

@implementation DownloadStartedAnimationMac

- (id)initWithTabContents:(TabContents*)tabContents {
  if ((self = [super init])) {
    // Load the image of the download arrow.
    ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
    NSImage* image = bundle.GetNativeImageNamed(IDR_DOWNLOAD_ANIMATION_BEGIN);

    // Figure out the positioning in the current tab. Try to position the layer
    // against the left edge, and three times the download image's height from
    // the bottom of the tab, assuming there is enough room. If there isn't
    // enough, don't show the animation and let the shelf speak for itself.
    gfx::Rect bounds;
    tabContents->GetContainerBounds(&bounds);
    imageWidth_ = [image size].width;
    CGFloat imageHeight = [image size].height;

    // Sanity check the size in case there's no room to display the animation.
    if (bounds.height() < imageHeight) {
      [self release];
      return nil;
    }

    NSView* tabContentsView = tabContents->GetNativeView();
    NSWindow* parentWindow = [tabContentsView window];
    if (!parentWindow) {
      // The tab is no longer frontmost.
      [self release];
      return nil;
    }

    NSPoint origin = [tabContentsView frame].origin;
    origin = [tabContentsView convertPoint:origin toView:nil];
    origin = [parentWindow convertBaseToScreen:origin];

    // Create the animation object to assist in animating and fading.
    CGFloat animationHeight = MIN(bounds.height(), 4 * imageHeight);
    NSRect frame = NSMakeRect(origin.x, origin.y, imageWidth_, animationHeight);
    animation_ = [[AnimatableImage alloc] initWithImage:image
                                         animationFrame:frame];
    [parentWindow addChildWindow:animation_ ordered:NSWindowAbove];

    animationHeight = MIN(bounds.height(), 3 * imageHeight);
    [animation_ setStartFrame:CGRectMake(0, animationHeight,
                                         imageWidth_, imageHeight)];
    [animation_ setEndFrame:CGRectMake(0, imageHeight,
                                       imageWidth_, imageHeight)];
    [animation_ setStartOpacity:1.0];
    [animation_ setEndOpacity:0.4];
    [animation_ setDuration:0.6];

    observer_.reset(new DownloadAnimationTabObserver(self, tabContents));

    // Set up to get notified about resize events on the parent window.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(parentWindowChanged:)
                   name:NSWindowDidResizeNotification
                 object:parentWindow];
    // When the animation window closes, it needs to be removed from the
    // parent window.
    [center addObserver:self
               selector:@selector(windowWillClose:)
                   name:NSWindowWillCloseNotification
                object:animation_];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

// Called when the parent window is resized.
- (void)parentWindowChanged:(NSNotification*)notification {
  NSWindow* parentWindow = [animation_ parentWindow];
  DCHECK([[notification object] isEqual:parentWindow]);
  NSRect parentFrame = [parentWindow frame];
  NSRect frame = parentFrame;
  frame.size.width = MIN(imageWidth_, NSWidth(parentFrame));
  [animation_ setFrame:frame display:YES];
}

- (void)closeAnimation {
  [animation_ close];
}

// When the animation closes, release self.
- (void)windowWillClose:(NSNotification*)notification {
  DCHECK([[notification object] isEqual:animation_]);
  [[animation_ parentWindow] removeChildWindow:animation_];
  [self release];
}

+ (void)startAnimationWithTabContents:(TabContents*)contents {
  // Will be deleted when the animation window closes.
  DownloadStartedAnimationMac* controller =
      [[self alloc] initWithTabContents:contents];
  // The initializer can return nil.
  if (!controller)
    return;

  // The |animation_| releases itself when done.
  [controller->animation_ startAnimation];
}

@end

void DownloadStartedAnimation::Show(TabContents* tab_contents) {
  DCHECK(tab_contents);

  // Will be deleted when the animation is complete.
  [DownloadStartedAnimationMac startAnimationWithTabContents:tab_contents];
}
