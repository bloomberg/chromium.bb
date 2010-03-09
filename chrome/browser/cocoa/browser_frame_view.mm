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

static const CGFloat kBrowserFrameViewPaintHeight = 60.0;
static const NSPoint kBrowserFrameViewPatternPhaseOffset = { -5, 3 };

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
  // If this isn't the window class we expect, then pass it on to the
  // original implementation.
  if (![[self window] isKindOfClass:[ChromeBrowserWindow class]]) {
    [self drawRectOriginal:rect];
    return;
  }

  // WARNING: There is an obvious optimization opportunity here that you DO NOT
  // want to take. To save painting cycles, you might think it would be a good
  // idea to call out to -drawRectOriginal: only if no theme were drawn. In
  // reality, however, if you fail to call -drawRectOriginal:, or if you call it
  // after a clipping path is set, the rounded corners at the top of the window
  // will not draw properly. Do not try to be smart here.

  // Only paint the top of the window.
  NSWindow* window = [self window];
  NSRect windowRect = [window frame];
  windowRect.origin = NSMakePoint(0, 0);

  NSRect paintRect = windowRect;
  paintRect.origin.y = NSMaxY(paintRect) - kBrowserFrameViewPaintHeight;
  paintRect.size.height = kBrowserFrameViewPaintHeight;
  rect = NSIntersectionRect(paintRect, rect);
  [self drawRectOriginal:rect];

  // Set up our clip.
  [[NSBezierPath bezierPathWithRoundedRect:windowRect
                                   xRadius:4
                                   yRadius:4] addClip];
  [[NSBezierPath bezierPathWithRect:rect] addClip];

  // Do the theming.
  BOOL themed = [BrowserFrameView drawWindowThemeInDirtyRect:rect
                                                     forView:self
                                                      bounds:windowRect];

  // Pinstripe the top.
  if (themed) {
    windowRect = [window frame];
    windowRect.origin = NSMakePoint(0, 0);
    windowRect.origin.y -= 0.5;
    windowRect.origin.x -= 0.5;
    windowRect.size.width += 1.0;
    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.5] set];
    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:windowRect
                                                         xRadius:4
                                                         yRadius:4];
    [path setLineWidth:1.0];
    [path stroke];
  }
}

+ (BOOL)drawWindowThemeInDirtyRect:(NSRect)dirtyRect
                           forView:(NSView*)view
                            bounds:(NSRect)bounds {
  ThemeProvider* themeProvider = [[view window] themeProvider];
  if (!themeProvider)
    return NO;

  BOOL active = [[view window] isMainWindow];
  BOOL incognito = [[view window] themeIsIncognito];

  // Find a theme image.
  NSColor* themeImageColor = nil;
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
    themeImageColor = themeProvider->GetNSImageColorNamed(themeImageID, true);

  // If no theme image, use a gradient if incognito.
  NSGradient* gradient = nil;
  if (!themeImageColor && incognito)
    gradient = themeProvider->GetNSGradient(
        active ? BrowserThemeProvider::GRADIENT_FRAME_INCOGNITO :
                 BrowserThemeProvider::GRADIENT_FRAME_INCOGNITO_INACTIVE);

  BOOL themed = NO;
  if (themeImageColor) {
    // The titlebar/tabstrip header on the mac is slightly smaller than on
    // Windows.  To keep the window background lined up with the tab and toolbar
    // patterns, we have to shift the pattern slightly, rather than simply
    // drawing it from the top left corner.  The offset below was empirically
    // determined in order to line these patterns up.
    //
    // This will make the themes look slightly different than in Windows/Linux
    // because of the differing heights between window top and tab top, but this
    // has been approved by UI.
    NSPoint phase = kBrowserFrameViewPatternPhaseOffset;
    phase.y += NSHeight(bounds);

    phase = [view convertPoint:phase toView:nil];

    [[NSGraphicsContext currentContext] setPatternPhase:phase];
    [themeImageColor set];
    NSRectFill(dirtyRect);
    themed = YES;
  } else if (gradient) {
    NSPoint startPoint = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
    NSPoint endPoint = startPoint;
    endPoint.y -= kBrowserFrameViewPaintHeight;
    [gradient drawFromPoint:startPoint toPoint:endPoint options:0];
    themed = YES;
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

  return themed;
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
