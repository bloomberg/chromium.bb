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
#include "base/scoped_nsobject.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_mac.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

class DownloadAnimationTabObserver;

// A class for managing the Core Animation download animation.
@interface DownloadStartedAnimationMac : NSObject {
  // The download arrow image which we are responsible for freeing.
  scoped_cftyperef<CGImageRef> image_;

  // The TabContents we will animate in (weak).
  TabContents* tabContents_;

  // The cocoa view object for our TabContents (weak).
  NSView* view_;

  // The observer for the TabContents we are drawing on.
  scoped_ptr<DownloadAnimationTabObserver> observer_;

  // Our animation layer.
  scoped_nsobject<CALayer> layer_;

  // Set once the animation is complete, either by interrupting (via window
  // close) or through normal a end of the animation.
  BOOL isComplete_;
};

// Called by our DownloadAnimationTabObserver if the tab is hidden or closed.
- (void)animationComplete;

@end  // interface DownloadStartedAnimationMac.


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

// Load the image of the download arrow.
- (id)initWithTabContents:(TabContents*)tabContents {
  if ((self = [super init])) {
    SkBitmap* image_bitmap =
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_DOWNLOAD_ANIMATION_BEGIN);
    image_.reset(SkCreateCGImageRef(*image_bitmap));
    tabContents_ = tabContents;
    view_ = tabContents_->GetContentNativeView();
    observer_.reset(new DownloadAnimationTabObserver(self, tabContents));
    isComplete_ = NO;
  }
  return self;
}

// Common clean up code.
- (void)animationComplete {
  if (isComplete_)
    return;
  isComplete_ = YES;
  [view_ setWantsLayer:NO];
  [layer_ removeAllAnimations];
  [layer_ removeFromSuperlayer];
}

// Set up the animation and let Core Animation do all the hard work.
- (void)animate {
  // Figure out the positioning in the current tab. We try to position ourselves
  // against the left edge, and three times the download image's height from the
  // bottom of the tab, assuming there is enough room. If there isn't enough, we
  // won't show the animation and let the shelf speak for itself.
  gfx::Rect bounds;
  tabContents_->GetContainerBounds(&bounds);
  int imageWidth = CGImageGetWidth(image_);
  int imageHeight = CGImageGetHeight(image_);
  CGRect imageBounds = CGRectMake(0, 0, imageWidth, imageHeight);

  // Sanity check the size in case there's no room to display the animation.
  if (bounds.height() < imageHeight) {
    [self release];
    return;
  }

  int animationHeight = std::min(bounds.height(), 3 * imageHeight);
  NSPoint start = NSMakePoint(0, animationHeight);
  NSPoint stop = NSMakePoint(0, 0);  // Bottom of the tab.

  // Set for the duration of the animation, or we won't see our layer. We reset
  // this in the completion callback. Layers by default scale their content to
  // fit, but we need them to clip their content instead, or else renderer
  // resizes will look janky.
  [view_ setWantsLayer:YES];
  [[view_ layer] setContentsGravity:kCAGravityTopLeft];

  // CALayer initalization.
  layer_.reset([[CALayer layer] retain]);
  [layer_ setNeedsDisplay];
  [layer_ setContents:(id)image_.get()];
  [layer_ setAnchorPoint:CGPointMake(0, 0)];
  [layer_ setFrame:imageBounds];
  [[view_ layer] addSublayer:layer_];

  // Positional animation.
  CABasicAnimation *animation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  [animation setFromValue:[NSValue valueWithPoint:start]];
  [animation setToValue:[NSValue valueWithPoint:stop]];
  [animation setDuration:0.6];
  CAMediaTimingFunction* mediaFunction =
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
  [animation setTimingFunction:mediaFunction];
  [animation setDelegate:self];
  [layer_ addAnimation:animation forKey:@"downloadPosition"];

  // Opacity animation.
  animation = [CABasicAnimation animationWithKeyPath:@"opacity"];
  [animation setFromValue:[NSNumber numberWithFloat:1.0]];
  [animation setToValue:[NSNumber numberWithFloat:0.0]];
  [animation setDuration:1.5];  // Slightly longer, so it doesn't fade too much.
  mediaFunction =
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
  [animation setTimingFunction:mediaFunction];
  [layer_ addAnimation:animation forKey:@"downloadOpacity"];
}

// CAAnimation delegate method called when the animation is complete.
- (void)animationDidStop:(CAAnimation *)animation finished:(BOOL)flag {
  [self animationComplete];
  [self release];
}

@end  // implementation DownloadStartedAnimationMac


// static
void DownloadStartedAnimation::Show(TabContents* tab_contents) {
  DCHECK(tab_contents);

  // Will be deleted when the animation is complete.
  DownloadStartedAnimationMac* downloadArrow =
      [[DownloadStartedAnimationMac alloc] initWithTabContents:tab_contents];

  // Go!
  [downloadArrow animate];
}
