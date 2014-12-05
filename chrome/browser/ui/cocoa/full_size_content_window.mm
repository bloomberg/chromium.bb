// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/full_size_content_window.h"

#include "base/logging.h"

@interface FullSizeContentWindow ()

+ (BOOL)shouldUseFullSizeContentViewForStyle:(NSUInteger)windowStyle;

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

@implementation FullSizeContentWindow

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
        [FullSizeContentWindow
            shouldUseFullSizeContentViewForStyle:windowStyle]) {
      chromeWindowView_.reset([[FullSizeContentView alloc] init]);
      [chromeWindowView_
          setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      [self setContentView:chromeWindowView_];
      [chromeWindowView_ setFrame:[[chromeWindowView_ superview] bounds]];

      // Our content view overlaps the window control buttons, so we must ensure
      // it is positioned below the buttons.
      NSView* superview = [chromeWindowView_ superview];
      [chromeWindowView_ removeFromSuperview];
      [superview addSubview:chromeWindowView_
                 positioned:NSWindowBelow
                 relativeTo:nil];
    }
  }
  return self;
}

#pragma mark - Private Methods

+ (BOOL)shouldUseFullSizeContentViewForStyle:(NSUInteger)windowStyle {
  return windowStyle & NSTitledWindowMask;
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
