// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_UI_ELEMENT_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_UI_ELEMENT_H_

#import <Foundation/Foundation.h>

@class FullscreenAnimator;

// UI elements that need to react to Fullscreen events should conform to this
// protocol to react to changes in Fullscreen state.
@protocol FullscreenUIElement<NSObject>

// Tells the UI to update its state for |progress|.  A fullscreen |progress|
// value denotes that the toolbar should be completely visible, and a |progress|
// value of 0.0 denotes that the toolbar should be completely hidden.
//
// NOTE: Since this selector is called for every scroll offset, it's not
// optional, as checking |-respondsToSelector:| for every FullscreenUIElement
// at every scroll offset can introduce performance issues.
- (void)updateForFullscreenProgress:(CGFloat)progress;

@optional

// Tells the UI to update its state after the max and min viewport insets have
// been updated to new values.  |progress| is the current progress value, and
// can be used to update the UI at the current progress with the new viewport
// inset range.
- (void)updateForFullscreenMinViewportInsets:(UIEdgeInsets)minViewportInsets
                           maxViewportInsets:(UIEdgeInsets)maxViewportInsets;

// Tells the UI that fullscreen is enabled or disabled.  When disabled, the UI
// should immediately be updated to the state corresponding with a progress
// value of 1.0.
- (void)updateForFullscreenEnabled:(BOOL)enabled;

// Called when fullscreen is about to initate an animation.  UI elements that
// react to fullscreen events should configure |animator| with animations.
- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_UI_ELEMENT_H_
