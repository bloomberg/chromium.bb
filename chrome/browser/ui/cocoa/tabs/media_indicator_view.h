// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_MEDIA_INDICATOR_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TABS_MEDIA_INDICATOR_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/tabs/tab_utils.h"

class MediaIndicatorViewAnimationDelegate;

namespace gfx {
class Animation;
}  // namespace gfx

@interface MediaIndicatorView : NSImageView {
 @private
  TabMediaState mediaState_;
  scoped_ptr<MediaIndicatorViewAnimationDelegate> delegate_;
  scoped_ptr<gfx::Animation> animation_;
  TabMediaState animatingMediaState_;
}

@property(readonly, nonatomic) TabMediaState mediaState;
@property(readonly, nonatomic) TabMediaState animatingMediaState;

// Initialize a new MediaIndicatorView in TAB_MEDIA_STATE_NONE (i.e., a
// non-active indicator).
- (id)init;

// Starts the animation to transition the indicator to the new |mediaState|.
- (void)updateIndicator:(TabMediaState)mediaState;

// Register a callback to be invoked whenever the animation completes.
- (void)setAnimationDoneCallbackObject:(id)anObject withSelector:(SEL)selector;

@end

@interface MediaIndicatorView(TestingAPI)
// Turns off animations for logic testing.
- (void)disableAnimations;
@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_MEDIA_INDICATOR_VIEW_H_
