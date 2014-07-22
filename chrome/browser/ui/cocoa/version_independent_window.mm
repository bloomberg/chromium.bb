// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/version_independent_window.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"

// This view always takes the size of its superview. It is intended to be used
// as a NSWindow's contentView.  It is needed because NSWindow's implementation
// explicitly resizes the contentView at inopportune times.
@interface FullSizeContentView : NSView
@end

namespace {

// Reorders the subviews of NSWindow's root view so that the contentView is
// moved to the back, and the ordering of the other views is unchanged.
// |context| should be an NSArray containing the subviews of the root view as
// they were previously ordered.
NSComparisonResult ReorderContentViewToBack(id firstView,
                                            id secondView,
                                            void* context) {
  NSView* contentView = [[firstView window] contentView];
  NSArray* subviews = static_cast<NSArray*>(context);
  if (firstView == contentView)
    return NSOrderedAscending;
  if (secondView == contentView)
    return NSOrderedDescending;
  NSUInteger index1 = [subviews indexOfObject:firstView];
  NSUInteger index2 = [subviews indexOfObject:secondView];
  return (index1 < index2) ? NSOrderedAscending : NSOrderedDescending;
}

}  // namespace

@interface VersionIndependentWindow ()

+ (BOOL)shouldUseFullSizeContentViewForStyle:(NSUInteger)windowStyle;

- (NSView*)chromeWindowView;

@end

@implementation FullSizeContentView

// This method is directly called by NSWindow during a window resize on OSX
// 10.10.0, beta 2. We must override it to prevent the content view from
// shrinking.
- (void)setFrameSize:(NSSize)size {
  if ([self superview])
    size = [[self superview] bounds].size;
  [super setFrameSize:size];
}

@end

@implementation NSWindow (VersionIndependentWindow)

- (NSView*)cr_windowView {
  if ([self isKindOfClass:[VersionIndependentWindow class]]) {
    VersionIndependentWindow* window =
        static_cast<VersionIndependentWindow*>(self);
    NSView* chromeWindowView = [window chromeWindowView];
    if (chromeWindowView)
      return chromeWindowView;
  }

  return [[self contentView] superview];
}

@end

@implementation VersionIndependentWindow

#pragma mark - Lifecycle

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation {
  self = [super initWithContentRect:contentRect
                          styleMask:windowStyle
                            backing:bufferingType
                              defer:deferCreation];
  if (self) {
    if ([VersionIndependentWindow
        shouldUseFullSizeContentViewForStyle:windowStyle]) {
      chromeWindowView_.reset([[FullSizeContentView alloc] init]);
      [chromeWindowView_
          setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      [self setContentView:chromeWindowView_];
      [chromeWindowView_ setFrame:[[[self contentView] superview] bounds]];

      // Move the content view to the back.
      // In Yosemite, the content view takes up the full size of the window,
      // and when it is in front of the zoom/fullscreen button, alt-clicking
      // the button has the wrong effect.
      // Adding subviews to the NSThemeFrame provokes a warning in Yosemite, so
      // we sort the subviews in place.
      // http://crbug.com/393808
      NSView* superview = [[self contentView] superview];
      base::scoped_nsobject<NSArray> subviews([[superview subviews] copy]);
      [superview sortSubviewsUsingFunction:&ReorderContentViewToBack
                                   context:subviews.get()];
    }
  }
  return self;
}

#pragma mark - Private Methods

+ (BOOL)shouldUseFullSizeContentViewForStyle:(NSUInteger)windowStyle {
  return (windowStyle & NSTitledWindowMask) && base::mac::IsOSYosemiteOrLater();
}

- (NSView*)chromeWindowView {
  return chromeWindowView_;
}

#pragma mark - NSWindow Overrides

#ifndef NDEBUG

- (void)setContentSize:(NSSize)size {
  DCHECK(!chromeWindowView_);
  [super setContentSize:size];
}

- (void)setContentMinSize:(NSSize)size {
  DCHECK(!chromeWindowView_);
  [super setContentMinSize:size];
}

- (void)setContentMaxSize:(NSSize)size {
  DCHECK(!chromeWindowView_);
  [super setContentMaxSize:size];
}

- (void)setContentAspectRatio:(NSSize)ratio {
  DCHECK(!chromeWindowView_);
  [super setContentAspectRatio:ratio];
}

#endif  // NDEBUG

+ (NSRect)frameRectForContentRect:(NSRect)cRect styleMask:(NSUInteger)aStyle {
  if ([self shouldUseFullSizeContentViewForStyle:aStyle])
    return cRect;
  return [super frameRectForContentRect:cRect styleMask:aStyle];
}

- (NSRect)frameRectForContentRect:(NSRect)contentRect {
  if (chromeWindowView_)
    return contentRect;
  return [super frameRectForContentRect:contentRect];
}

+ (NSRect)contentRectForFrameRect:(NSRect)fRect styleMask:(NSUInteger)aStyle {
  if ([self shouldUseFullSizeContentViewForStyle:aStyle])
    return fRect;
  return [super contentRectForFrameRect:fRect styleMask:aStyle];
}

- (NSRect)contentRectForFrameRect:(NSRect)frameRect {
  if (chromeWindowView_)
    return frameRect;
  return [super contentRectForFrameRect:frameRect];
}

@end
