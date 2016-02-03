// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/full_size_content_window.h"

#include <crt_externs.h>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_objc_class_swizzler.h"

@interface FullSizeContentWindow ()

+ (BOOL)shouldUseFullSizeContentViewForStyle:(NSUInteger)windowStyle;

@end

// This view always takes the size of its superview. It is intended to be used
// as a NSWindow's contentView.  It is needed because NSWindow's implementation
// explicitly resizes the contentView at inopportune times.
@interface FullSizeContentView : NSView {
  BOOL forceFrameFlag_;
}

// This method allows us to set the content view size since setFrameSize is
// overridden to prevent the view from shrinking.
- (void)forceFrame:(NSRect)frame;

@end

@implementation FullSizeContentView

// This method is directly called by AppKit during a live window resize.
// Override it to prevent the content view from shrinking.
- (void)setFrameSize:(NSSize)size {
  if ([self superview] && !forceFrameFlag_)
    size = [[self superview] bounds].size;
  [super setFrameSize:size];
}

- (void)forceFrame:(NSRect)frame {
  forceFrameFlag_ = YES;
  [super setFrame:frame];
  forceFrameFlag_ = NO;
}

@end

static bool g_disable_callstacksymbols = false;
static IMP g_original_callstacksymbols_implementation;

@interface FullSizeContentWindowSwizzlingSupport : NSObject
@end

@implementation FullSizeContentWindowSwizzlingSupport

// This method replaces [NSThread callStackSymbols] via swizzling - see +load
// below.
+ (NSArray*)callStackSymbols {
  return g_disable_callstacksymbols ?
      @[@"+callStackSymbols disabled for performance reasons"] :
      g_original_callstacksymbols_implementation(
          self, @selector(callStackSymbols));
}

@end

@implementation FullSizeContentWindow

#pragma mark - Lifecycle

// In initWithContentRect:styleMask:backing:defer:, the call to
// [NSView addSubview:positioned:relativeTo:] causes NSWindow to complain that
// an unknown view is being added to it, and to generate a stack trace.
// Not only does this stack trace pollute the console, it can also take hundreds
// of milliseconds to generate (because of symbolication). By swizzling
// [NSThread callStackSymbols] we can prevent the stack trace output.
// See crbug.com/520373 .
+ (void)load {
  // Swizzling should only happen in the browser process.
  const char* const* const argv = *_NSGetArgv();
  const int argc = *_NSGetArgc();
  const char kType[] = "--type=";
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (strncmp(arg, kType, strlen(kType)) == 0) {
      return;
    }
  }

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    Class targetClass = [NSThread class];
    Class swizzleClass = [FullSizeContentWindowSwizzlingSupport class];
    SEL targetSelector = @selector(callStackSymbols);

    CR_DEFINE_STATIC_LOCAL(base::mac::ScopedObjCClassSwizzler,
                           callStackSymbolsSuppressor, (targetClass,
                           swizzleClass, targetSelector));
    g_original_callstacksymbols_implementation =
        callStackSymbolsSuppressor.GetOriginalImplementation();
  });
}

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

      // Prevent the AppKit from generating a backtrace to include in it's
      // complaint about our upcoming call to addSubview:positioned:relativeTo:.
      // See +load for more info.
      base::AutoReset<bool> disable_symbolication(&g_disable_callstacksymbols,
                                                  true);

      [superview addSubview:chromeWindowView_
                 positioned:NSWindowBelow
                 relativeTo:nil];
    }
  }
  return self;
}

- (void)forceContentViewFrame:(NSRect)frame {
  if ([chromeWindowView_ isKindOfClass:[FullSizeContentView class]]) {
    FullSizeContentView* contentView =
        base::mac::ObjCCast<FullSizeContentView>(chromeWindowView_);
    [contentView forceFrame:frame];
  } else if (chromeWindowView_) {
    [chromeWindowView_ setFrame:frame];
  } else {
    [self.contentView setFrame:frame];
  }
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
