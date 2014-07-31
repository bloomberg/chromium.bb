// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/version_independent_window.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "chrome/common/chrome_switches.h"

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

// This method is directly called by NSWindow during a window resize on OSX
// 10.10.0, beta 2. We must override it to prevent the content view from
// shrinking.
- (void)setFrameSize:(NSSize)size {
  if ([self superview])
    size = [[self superview] bounds].size;
  [super setFrameSize:size];
}

// The contentView gets moved around during certain full-screen operations.
// This is less than ideal, and should eventually be removed.
- (void)viewDidMoveToSuperview {
  [self setFrame:[[self superview] bounds]];
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
      [chromeWindowView_ setFrame:[[[self contentView] superview] bounds]];
      [self setContentView:chromeWindowView_];
    }
  }
  return self;
}

#pragma mark - Private Methods

+ (BOOL)shouldUseFullSizeContentViewForStyle:(NSUInteger)windowStyle {
  // TODO(erikchen): Once OSX Yosemite is released, consider removing this
  // class entirely.
  // http://crbug.com/398574
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableFullSizeContentView))
    return NO;
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
