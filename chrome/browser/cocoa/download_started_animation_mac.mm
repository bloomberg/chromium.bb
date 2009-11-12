// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the Mac implementation the download animation, displayed
// at the start of a download. The animation produces an arrow pointing
// downwards and animates towards the bottom of the window where the new
// download appears in the download shelf.

#include "chrome/browser/download/download_started_animation.h"

#import <QuartzCore/QuartzCore.h>

#include "app/resource_bundle.h"
#include "base/scoped_cftyperef.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_mac.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

class DownloadAnimationTabObserver;

// A class for managing the Core Animation download animation.
// Should be instantiated using +startAnimationWithTabContents:.
@interface DownloadStartedAnimationMac : NSWindow {
 @private
  // The observer for the TabContents we are drawing on.
  scoped_ptr<DownloadAnimationTabObserver> observer_;
  CGFloat imageWidth_;
};

+ (void)startAnimationWithTabContents:(TabContents*)tabContents;
// Called by our DownloadAnimationTabObserver if the tab is hidden or closed.
- (void)animationComplete;

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
    registrar_.Remove(this,
                      NotificationType::TAB_CONTENTS_HIDDEN,
                      Source<TabContents>(tab_contents_));
    registrar_.Remove(this,
                      NotificationType::TAB_CONTENTS_DESTROYED,
                      Source<TabContents>(tab_contents_));
    [owner_ animationComplete];
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
  // Load the image of the download arrow.
  ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
  SkBitmap* imageBitmap = bundle.GetBitmapNamed(IDR_DOWNLOAD_ANIMATION_BEGIN);
  scoped_cftyperef<CGImageRef> image(SkCreateCGImageRef(*imageBitmap));

  // Figure out the positioning in the current tab. Try to position the layer
  // against the left edge, and three times the download image's height from the
  // bottom of the tab, assuming there is enough room. If there isn't enough,
  // don't show the animation and let the shelf speak for itself.
  gfx::Rect bounds;
  tabContents->GetContainerBounds(&bounds);
  imageWidth_ = CGImageGetWidth(image);
  CGFloat imageHeight = CGImageGetHeight(image);
  CGRect imageBounds = CGRectMake(0, 0, imageWidth_, imageHeight);

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
  origin = [tabContentsView convertPointToBase:origin];
  origin = [parentWindow convertBaseToScreen:origin];

  // Create a window to host a layer that animates the sliding and fading.
  CGFloat animationHeight = MIN(bounds.height(), 4 * imageHeight);
  NSRect frame = NSMakeRect(origin.x, origin.y, imageWidth_, animationHeight);
  if ((self = [super initWithContentRect:frame
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:NO])) {
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setIgnoresMouseEvents:YES];

    // Must be set or else self will be leaked.
    [self setReleasedWhenClosed:YES];

    // Set up to get notified about resize events on the parent window.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(parentWindowChanged:)
                   name:NSWindowDidResizeNotification
                 object:parentWindow];
    [parentWindow addChildWindow:self ordered:NSWindowAbove];

    // Set up the root layer. By calling -setLayer: followed by -setWantsLayer:
    // the view becomes a layer hosting view as opposed to a layer backed view.
    NSView* view = [self contentView];
    CALayer* rootLayer = [CALayer layer];
    [view setLayer:rootLayer];
    [view setWantsLayer:YES];

    // Create the layer that will be animated.
    CALayer* layer = [CALayer layer];
    [layer setContents:(id)image.get()];
    [layer setAnchorPoint:CGPointMake(0, 1)];
    [layer setFrame:CGRectMake(0, 0, imageWidth_, imageHeight)];
    [layer setNeedsDisplayOnBoundsChange:YES];
    [rootLayer addSublayer:layer];

    // Common timing function for all animations.
    CAMediaTimingFunction* mediaFunction =
        [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];

    // Positional animation.
    CABasicAnimation* positionAnimation =
        [CABasicAnimation animationWithKeyPath:@"position"];
    CGFloat animationHeight = MIN(bounds.height(), 3 * imageHeight);
    NSPoint start = NSMakePoint(0, animationHeight);
    NSPoint stop = NSMakePoint(0, imageHeight);
    [positionAnimation setFromValue:[NSValue valueWithPoint:start]];
    [positionAnimation setToValue:[NSValue valueWithPoint:stop]];
    [positionAnimation gtm_setDuration:0.6];
    [positionAnimation setTimingFunction:mediaFunction];

    // Opacity animation.
    CABasicAnimation* opacityAnimation =
        [CABasicAnimation animationWithKeyPath:@"opacity"];
    [opacityAnimation setFromValue:[NSNumber numberWithFloat:1.0]];
    [opacityAnimation setToValue:[NSNumber numberWithFloat:0.4]];
    [opacityAnimation gtm_setDuration:0.6];
    [opacityAnimation setTimingFunction:mediaFunction];

    // Group the animations together.
    CAAnimationGroup* animationGroup = [CAAnimationGroup animation];
    NSArray* animations =
        [NSArray arrayWithObjects:positionAnimation, opacityAnimation, nil];
    [animationGroup setAnimations:animations];

    // Set self as delegate so self receives -animationDidStop:finished:;
    [animationGroup setDelegate:self];
    [animationGroup setTimingFunction:mediaFunction];
    [animationGroup gtm_setDuration:0.6];
    [layer addAnimation:animationGroup forKey:@"downloadOpacityAndPosition"];

    observer_.reset(new DownloadAnimationTabObserver(self, tabContents));
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

// Called when the parent window is resized.
- (void)parentWindowChanged:(NSNotification*)notification {
  NSWindow* parentWindow = [self parentWindow];
  DCHECK([[notification object] isEqual:parentWindow]);
  NSRect parentFrame = [parentWindow frame];
  NSRect frame = parentFrame;
  frame.size.width = MIN(imageWidth_, NSWidth(parentFrame));
  [self setFrame:frame display:YES];
}

// CAAnimation delegate method called when the animation is complete.
- (void)animationDidStop:(CAAnimation *)animation finished:(BOOL)flag {
  [self animationComplete];
}

// Common clean up code.
- (void)animationComplete {
  [[self parentWindow] removeChildWindow:self];
  [self close];
}

+ (void)startAnimationWithTabContents:(TabContents*)contents {
  // Will be deleted when the animation is complete in -animationComplete.
  [[self alloc] initWithTabContents:contents];
}

@end

void DownloadStartedAnimation::Show(TabContents* tab_contents) {
  DCHECK(tab_contents);

  // Will be deleted when the animation is complete.
  [DownloadStartedAnimationMac startAnimationWithTabContents:tab_contents];
}
