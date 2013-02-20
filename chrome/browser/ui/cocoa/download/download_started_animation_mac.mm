// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the Mac implementation the download animation, displayed
// at the start of a download. The animation produces an arrow pointing
// downwards and animates towards the bottom of the window where the new
// download appears in the download shelf.

#include "chrome/browser/download/download_started_animation.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/animatable_image.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

class DownloadAnimationWebObserver;

using content::WebContents;

// A class for managing the Core Animation download animation.
// Should be instantiated using +startAnimationWithWebContents:.
@interface DownloadStartedAnimationMac : NSObject {
 @private
  // The observer for the WebContents we are drawing on.
  scoped_ptr<DownloadAnimationWebObserver> observer_;
  CGFloat imageWidth_;
  AnimatableImage* animation_;
};

+ (void)startAnimationWithWebContents:(WebContents*)webContents;

// Called by the Observer if the tab is hidden or closed.
- (void)closeAnimation;

@end

// A helper class to monitor tab hidden and closed notifications. If we receive
// such a notification, we stop the animation.
class DownloadAnimationWebObserver : public content::NotificationObserver {
 public:
  DownloadAnimationWebObserver(DownloadStartedAnimationMac* owner,
                               WebContents* web_contents)
      : owner_(owner),
        web_contents_(web_contents) {
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
                   content::Source<WebContents>(web_contents_));
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   content::Source<WebContents>(web_contents_));
  }

  // Runs when a tab is hidden or destroyed. Let our owner know we should end
  // the animation.
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE {
    if (type == content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED) {
      bool visible = *content::Details<bool>(details).ptr();
      if (visible)
        return;
    }
    // This ends up deleting us.
    [owner_ closeAnimation];
  }

 private:
  // The object we need to inform when we get a notification. Weak.
  DownloadStartedAnimationMac* owner_;

  // The tab we are observing. Weak.
  WebContents* web_contents_;

  // Used for registering to receive notifications and automatic clean up.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DownloadAnimationWebObserver);
};

@implementation DownloadStartedAnimationMac

- (id)initWithWebContents:(WebContents*)webContents {
  if ((self = [super init])) {
    // Load the image of the download arrow.
    ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
    NSImage* image =
        bundle.GetNativeImageNamed(IDR_DOWNLOAD_ANIMATION_BEGIN).ToNSImage();

    // Figure out the positioning in the current tab. Try to position the layer
    // against the left edge, and three times the download image's height from
    // the bottom of the tab, assuming there is enough room. If there isn't
    // enough, don't show the animation and let the shelf speak for itself.
    gfx::Rect bounds;
    webContents->GetContainerBounds(&bounds);
    imageWidth_ = [image size].width;
    CGFloat imageHeight = [image size].height;

    // Sanity check the size in case there's no room to display the animation.
    if (bounds.height() < imageHeight) {
      [self release];
      return nil;
    }

    NSView* tabContentsView = webContents->GetNativeView();
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

    observer_.reset(new DownloadAnimationWebObserver(self, webContents));

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

+ (void)startAnimationWithWebContents:(WebContents*)contents {
  // Will be deleted when the animation window closes.
  DownloadStartedAnimationMac* controller =
      [[self alloc] initWithWebContents:contents];
  // The initializer can return nil.
  if (!controller)
    return;

  // The |animation_| releases itself when done.
  [controller->animation_ startAnimation];
}

@end

void DownloadStartedAnimation::Show(WebContents* web_contents) {
  DCHECK(web_contents);

  // Will be deleted when the animation is complete.
  [DownloadStartedAnimationMac startAnimationWithWebContents:web_contents];
}
