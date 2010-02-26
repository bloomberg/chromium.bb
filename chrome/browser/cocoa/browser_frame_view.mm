// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/browser_frame_view.h"

#import <objc/runtime.h>
#import <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"
#import "chrome/browser/browser_theme_provider.h"
#import "chrome/browser/cocoa/chrome_browser_window.h"
#import "chrome/browser/cocoa/themed_window.h"
#include "grit/theme_resources.h"

@interface NSView (Swizzles)
- (void)drawRectOriginal:(NSRect)rect;
- (BOOL)_mouseInGroup:(NSButton*)widget;
- (void)updateTrackingAreas;
@end

@implementation BrowserFrameView

+ (void)load {
  // This is where we swizzle drawRect, and add in two methods that we
  // need. If any of these fail it shouldn't affect the functionality of the
  // others. If they all fail, we will lose window frame theming and
  // roll overs for our close widgets, but things should still function
  // correctly.
  base::ScopedNSAutoreleasePool pool;
  Class grayFrameClass = NSClassFromString(@"NSGrayFrame");
  DCHECK(grayFrameClass);
  if (!grayFrameClass) return;

  // Exchange draw rect
  Method m0 = class_getInstanceMethod([self class], @selector(drawRect:));
  DCHECK(m0);
  if (m0) {
    BOOL didAdd = class_addMethod(grayFrameClass,
                                  @selector(drawRectOriginal:),
                                  method_getImplementation(m0),
                                  method_getTypeEncoding(m0));
    DCHECK(didAdd);
    if (didAdd) {
      Method m1 = class_getInstanceMethod(grayFrameClass, @selector(drawRect:));
      Method m2 = class_getInstanceMethod(grayFrameClass,
                                          @selector(drawRectOriginal:));
      DCHECK(m1 && m2);
      if (m1 && m2) {
        method_exchangeImplementations(m1, m2);
      }
    }
  }

  // Add _mouseInGroup
  m0 = class_getInstanceMethod([self class], @selector(_mouseInGroup:));
  DCHECK(m0);
  if (m0) {
    BOOL didAdd = class_addMethod(grayFrameClass,
                                  @selector(_mouseInGroup:),
                                  method_getImplementation(m0),
                                  method_getTypeEncoding(m0));
    DCHECK(didAdd);
  }
  // Add updateTrackingArea
  m0 = class_getInstanceMethod([self class], @selector(updateTrackingAreas));
  DCHECK(m0);
  if (m0) {
    BOOL didAdd = class_addMethod(grayFrameClass,
                                  @selector(updateTrackingAreas),
                                  method_getImplementation(m0),
                                  method_getTypeEncoding(m0));
    DCHECK(didAdd);
  }
}

- (id)initWithFrame:(NSRect)frame {
  // This class is not for instantiating.
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (id)initWithCoder:(NSCoder*)coder {
  // This class is not for instantiating.
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

// Here is our custom drawing for our frame.
- (void)drawRect:(NSRect)rect {
  [self drawRectOriginal:rect];
  // If this isn't the window class we expect, then pass it on to the
  // original implementation.
  if (![[self window] isKindOfClass:[ChromeBrowserWindow class]]) {
    return;
  }

  // Set up our clip.
  NSWindow* window = [self window];
  NSRect windowRect = [window frame];
  windowRect.origin = NSMakePoint(0, 0);
  [[NSBezierPath bezierPathWithRoundedRect:windowRect
                                   xRadius:4
                                   yRadius:4] addClip];
  [[NSBezierPath bezierPathWithRect:rect] addClip];

  // Do the theming.
  [BrowserFrameView drawWindowThemeInDirtyRect:rect
                                       forView:self
                                        bounds:windowRect];
}

+ (void)drawWindowThemeInDirtyRect:(NSRect)dirtyRect
                           forView:(NSView*)view
                            bounds:(NSRect)bounds {
  ThemeProvider* themeProvider = [[view window] themeProvider];
  if (!themeProvider)
    return;

  BOOL active = [[view window] isMainWindow];
  BOOL incognito = [[view window] themeIsIncognito];

  // Find a theme image.
  NSImage* themeImage = nil;
  int themeImageID;
  if (active && incognito)
    themeImageID = IDR_THEME_FRAME_INCOGNITO;
  else if (active && !incognito)
    themeImageID = IDR_THEME_FRAME;
  else if (!active && incognito)
    themeImageID = IDR_THEME_FRAME_INCOGNITO_INACTIVE;
  else
    themeImageID = IDR_THEME_FRAME_INACTIVE;
  if (themeProvider->HasCustomImage(IDR_THEME_FRAME))
    themeImage = themeProvider->GetNSImageNamed(themeImageID, true);

  // If no theme image, use a gradient if incognito.
  NSGradient* gradient = nil;
  if (!themeImage && incognito)
    gradient = themeProvider->GetNSGradient(
        active ? BrowserThemeProvider::GRADIENT_FRAME_INCOGNITO :
                 BrowserThemeProvider::GRADIENT_FRAME_INCOGNITO_INACTIVE);

  if (themeImage) {
    NSColor* themeImageColor = [NSColor colorWithPatternImage:themeImage];

    // The titlebar/tabstrip header on the mac is slightly smaller than on
    // Windows.  To keep the window background lined up with the tab and toolbar
    // patterns, we have to shift the pattern slightly, rather than simply
    // drawing it from the top left corner.  The offset below was empirically
    // determined in order to line these patterns up.
    //
    // This will make the themes look slightly different than in Windows/Linux
    // because of the differing heights between window top and tab top, but this
    // has been approved by UI.
    static const NSPoint kBrowserFrameViewPatternPhaseOffset = { -5, 3 };
    NSPoint phase = kBrowserFrameViewPatternPhaseOffset;
    phase.y += NSHeight(bounds);

    phase = [view convertPoint:phase toView:nil];

    [[NSGraphicsContext currentContext] setPatternPhase:phase];
    [themeImageColor set];
    NSRectFill(dirtyRect);
  } else if (gradient) {
    // Only paint the gradient at the top of the window. (This is at the maximum
    // when fullscreening; before adjusting check this case.)
    static const CGFloat kBrowserFrameViewGradientHeight = 52.0;
    NSRect gradientRect = bounds;
    gradientRect.origin.y = NSMaxY(gradientRect) -
        kBrowserFrameViewGradientHeight;
    gradientRect.size.height = kBrowserFrameViewGradientHeight;

    [gradient drawInRect:gradientRect angle:270];
  }

  // Check to see if we have an overlay image.
  NSImage* overlayImage =
      themeProvider->GetNSImageNamed(active ? IDR_THEME_FRAME_OVERLAY :
                                              IDR_THEME_FRAME_OVERLAY_INACTIVE,
                                     false);
  if (overlayImage) {
    // Anchor to top-left and don't scale.
    NSSize overlaySize = [overlayImage size];
    NSRect imageFrame = NSMakeRect(0, 0, overlaySize.width, overlaySize.height);
    [overlayImage drawAtPoint:NSMakePoint(0, NSHeight(bounds) -
                                               overlaySize.height)
                     fromRect:imageFrame
                    operation:NSCompositeSourceOver
                     fraction:1.0];
  }
}

// Check to see if the mouse is currently in one of our window widgets.
- (BOOL)_mouseInGroup:(NSButton*)widget {
  BOOL mouseInGroup = NO;
  if ([[self window] isKindOfClass:[ChromeBrowserWindow class]]) {
    ChromeBrowserWindow* window =
        static_cast<ChromeBrowserWindow*>([self window]);
    mouseInGroup = [window mouseInGroup:widget];
  } else if ([super respondsToSelector:@selector(_mouseInGroup:)]) {
    mouseInGroup = [super _mouseInGroup:widget];
  }
  return mouseInGroup;
}

// Let our window handle updating the window widget tracking area.
- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  if ([[self window] isKindOfClass:[ChromeBrowserWindow class]]) {
    ChromeBrowserWindow* window =
        static_cast<ChromeBrowserWindow*>([self window]);
    [window updateTrackingAreas];
  }
}

@end
