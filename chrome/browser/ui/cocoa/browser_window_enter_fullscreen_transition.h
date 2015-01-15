// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_FULLSCREEN_TRANSITION_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_FULLSCREEN_TRANSITION_H_

#import <Cocoa/Cocoa.h>

// This class is responsible for managing the custom transition of a
// BrowserWindow from its normal state into an AppKit Fullscreen state.
//
// By default, when AppKit Fullscreens a window, it creates a new virtual
// desktop and slides it in from the right of the screen. At the same time, the
// old virtual desktop slides off to the left. This animation takes one second,
// and the time is not customizable without elevated privileges or a
// self-modifying binary
// (https://code.google.com/p/chromium/issues/detail?id=414527). During that
// second, no user interaction is possible.
//
// The default implementation of the AppKit transition smoothly animates a
// window from its original size to the full size of the screen. At the
// beginning of the animation, it takes a snapshot of the window's current
// state. Then it resizes the window, calls drawRect: (theorized, not tested),
// and takes a snapshot of the window's final state. The animation is a simple
// crossfade between the two snapshots. This has a flaw. Frequently, the
// renderer has not yet drawn content for the resized window by the time
// drawRect: is called. As a result, the animation is effectively a no-op. When
// the animation is finished, the new web content flashes in.
//
// The window's delegate can override two methods to customize the transition.
//  -customWindowsToEnterFullScreenForWindow:
//    The return of this method is an array of NSWindows. Each window that is
//    returned will be added to the new virtual desktop after the animation is
//    finished, but will not be a part of the animation itself.
//  -window:startCustomAnimationToEnterFullScreenWithDuration:
//    In this method, the window's delegate adds animations to the windows
//    returned in the above method.
//
// The goal of this class is to mimic the default animation, but instead of
// taking a snapshot of the final web content, it uses the live web content
// during the animation.
//
// See https://code.google.com/p/chromium/issues/detail?id=414527#c22 and its
// preceding comments for a more detailed description of the implementation,
// and the reasoning behind the decisions made.
//
// Recommended usage:
//  (Override method on NSWindow's delegate)
//  - (NSArray*)customWindowsToEnterFullScreenForWindow:(NSWindow*)window {
//    self.transition = [[[BrowserWindowEnterFullscreenTransition alloc]
//        initWithWindow:window] autorelease];
//    return [self.transition customWindowsToEnterFullScreen];
//  }
//
//  (Override method on NSWindow's delegate)
//  - (void)window:(NSWindow*)window
//  startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration {
//    [self.transition
//        startCustomAnimationToEnterFullScreenWithDuration:duration];
//  }
//
//  (Override method on NSWindow's delegate)
//  - (void)windowDidEnterFullScreen:(NSNotification*)notification {
//    self.transition = nil;
//  }
//
//  (Override method on NSWindow)
//  - (NSRect)constrainFrameRect:(NSRect)frame toScreen:(NSScreen*)screen {
//    if (self.transition && ![self.transition shouldWindowBeConstrained])
//      return frame;
//    return [super constrainFrameRect:frame toScreen:screen];
//  }

@interface BrowserWindowEnterFullscreenTransition : NSObject

// Designated initializer. |window| is the NSWindow that is going to be moved
// into a fullscreen Space (virtual desktop), and resized to have the same size
// as the screen. |window|'s root view must be layer backed.
- (instancetype)initWithWindow:(NSWindow*)window;

// Returns the windows to be used in the custom transition.
//   - Takes a snapshot of the current window.
//   - Makes a new snapshot window which shows the snapshot in the same
//   location as the current window.
//   - Adds the style mask NSFullScreenWindowMask to the current window.
//   - Makes the current window transparent, and resizes the current window to
//   be the same size as the screen.
- (NSArray*)customWindowsToEnterFullScreen;

// Begins the animations used for the custom fullscreen transition.
//   - Animates the snapshot to the full size of the screen while fading it out.
//   - Animates the current window from it's original location to its final
//   location, while fading it in.
// Note: The two animations are added to different layers in different windows.
// There is no explicit logic to keep the two animations in sync. If this
// proves to be a problem, the relevant layers should attempt to sync up their
// time offsets with CACurrentMediaTime().
- (void)startCustomAnimationToEnterFullScreenWithDuration:
    (NSTimeInterval)duration;

// When this method returns true, the NSWindow method
// -constrainFrameRect:toScreen: must return the frame rect without
// constraining it. The owner of the instance of this class is responsible for
// hooking up this logic.
- (BOOL)shouldWindowBeUnconstrained;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_FULLSCREEN_TRANSITION_H_
