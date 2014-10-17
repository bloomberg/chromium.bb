// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/version_independent_window.h"

#include "base/logging.h"

@interface VersionIndependentWindow ()

+ (BOOL)shouldUseFullSizeContentViewForStyle:(NSUInteger)windowStyle;

- (NSView*)chromeWindowView;

@end

// This view always takes the size of its superview. It is intended to be used
// as a NSWindow's contentView.  It is needed because NSWindow's implementation
// explicitly resizes the contentView at inopportune times.
@interface FullSizeContentView : NSView
@end

@implementation FullSizeContentView

// This method is directly called by AppKit during a live window resize.
// Override it to prevent the content view from shrinking.
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
  return [self initWithContentRect:contentRect
                         styleMask:windowStyle
                           backing:bufferingType
                             defer:deferCreation
            wantsViewsOverTitlebar:NO];
}

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation
             wantsViewsOverTitlebar:(BOOL)wantsViewsOverTitlebar {
  self = [super initWithContentRect:contentRect
                          styleMask:windowStyle
                            backing:bufferingType
                              defer:deferCreation];
  if (self) {
    if (wantsViewsOverTitlebar &&
        [VersionIndependentWindow
            shouldUseFullSizeContentViewForStyle:windowStyle]) {
      chromeWindowView_.reset([[FullSizeContentView alloc] init]);
      [chromeWindowView_
          setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      [self setContentView:chromeWindowView_];
      [chromeWindowView_ setFrame:[[chromeWindowView_ superview] bounds]];
    }
  }
  return self;
}

#pragma mark - Private Methods

+ (BOOL)shouldUseFullSizeContentViewForStyle:(NSUInteger)windowStyle {
  return windowStyle & NSTitledWindowMask;
}

- (NSView*)chromeWindowView {
  return chromeWindowView_;
}

#pragma mark - NSWindow Overrides

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
