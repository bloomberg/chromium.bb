// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_MEDIA_INDICATOR_BUTTON_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_TABS_MEDIA_INDICATOR_BUTTON_COCOA_H_

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#import "ui/base/cocoa/hover_button.h"

namespace gfx {
class Animation;
class AnimationDelegate;
}  // namespace gfx

// This is an HoverButton subclass that serves as both the media indicator icon
// (audio, tab capture, etc.), and as a mute button.  It is meant to only be
// used as a subview of TabView.
//
// When the indicator is transitioned to the audio playing or muting state, the
// button functionality is enabled and begins handling mouse events.  Otherwise,
// this view behaves like an image and all mouse events will be handled by the
// its superview.
//
// Note: Send the |-setClickTarget:withAction:| message instead of the
// |-setTarget:| and |-setAction:| messages to be notified of button clicks.
@interface MediaIndicatorButton : HoverButton {
 @private
  class FadeAnimationDelegate;

  // Current TabMediaState.  When animating fade-in/out, this reflects the
  // indicator state at the end of the animation.
  TabMediaState mediaState_;

  // Media indicator fade-in/out animation (i.e., only on show/hide, not a
  // continuous animation).
  scoped_ptr<gfx::AnimationDelegate> fadeAnimationDelegate_;
  scoped_ptr<gfx::Animation> fadeAnimation_;
  TabMediaState showingMediaState_;

  // Target and action invoked whenever a fade-in/out animation completes.  This
  // is used by TabController to layout the TabView after an indicator has
  // completely faded out.
  id animationDoneTarget_;  // weak
  SEL animationDoneAction_;

  // The image to show when the mouse hovers over the button.
  base::scoped_nsobject<NSImage> affordanceImage_;

  // Target and action invoked whenever an enabled button is clicked.
  id clickTarget_;  // weak
  SEL clickAction_;
}

@property(readonly, nonatomic) TabMediaState showingMediaState;

// Initialize a new MediaIndicatorButton in TAB_MEDIA_STATE_NONE (i.e., a
// non-active indicator).
- (id)init;

// Updates button images, starts fade animations, and activates/deactivates
// button functionality as appropriate.
- (void)transitionToMediaState:(TabMediaState)nextState;

// Determines whether the MediaIndicatorButtonCocoa will be clickable for
// toggling muting.  This should be called whenever the frame of this view is
// changed, and also whenever the active/inactive state of the tab has changed.
// Internally, |-transitionToMediaState:| will call this.
- (void)updateEnabledForMuteToggle;

// Register a message be sent to |target| whenever fade animations complete.  A
// weak reference on |target| is held.
- (void)setAnimationDoneTarget:(id)target withAction:(SEL)action;

// Request a message be sent to |target| whenever the enabled button has been
// clicked.  A weak reference on |target| is held.
- (void)setClickTarget:(id)target withAction:(SEL)action;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_MEDIA_INDICATOR_BUTTON_COCOA_H_
