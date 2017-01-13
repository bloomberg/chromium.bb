// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_strip_background_view.h"

#include "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/theme_provider.h"

// TODO(sdy): Remove once we no longer support 10.9 (-Wunguarded-availability).
@class NSVisualEffectView;
extern NSString* const NSAppearanceNameVibrantDark;
extern NSString* const NSAppearanceNameVibrantLight;

@interface NSAppearance (AllowsVibrancy)
@property(readonly) BOOL allowsVibrancy;
@end
// /TODO

@interface TabStripThemeBackgroundView : NSView
@property(nonatomic) BOOL inATabDraggingOverlayWindow;
@end

@implementation TabStripThemeBackgroundView

@synthesize inATabDraggingOverlayWindow = inATabDraggingOverlayWindow_;

- (void)drawRect:(NSRect)dirtyRect {
  // Only the top corners are rounded. For simplicity, round all 4 corners but
  // draw the bottom corners outside of the visible bounds.
  float cornerRadius = 4.0;
  bool isFullscreen = (self.window.styleMask & NSFullScreenWindowMask) != 0;

  NSRect roundedRect = [self bounds];
  if (!isFullscreen) {
    roundedRect.origin.y -= cornerRadius;
    roundedRect.size.height += cornerRadius;
    [[NSBezierPath bezierPathWithRoundedRect:roundedRect
                                     xRadius:cornerRadius
                                     yRadius:cornerRadius] addClip];
  }
  BOOL themed = [FramedBrowserWindow drawWindowThemeInDirtyRect:dirtyRect
                                                        forView:self
                                                         bounds:roundedRect
                                           forceBlackBackground:NO];

  // Draw a 1px border on the top edge and top corners.
  if (themed) {
    if (!isFullscreen) {
      CGFloat lineWidth = [self cr_lineWidth];
      // Inset the vertical lines by 0.5px so that the top line gets a full
      // pixel. Outset the horizontal lines by 0.5px so that they are not
      // visible, but still get the rounded corners to get a border.
      NSRect strokeRect =
          NSInsetRect(roundedRect, -lineWidth / 2, lineWidth / 2);
      NSBezierPath* path =
          [NSBezierPath bezierPathWithRoundedRect:strokeRect
                                          xRadius:cornerRadius
                                          yRadius:cornerRadius];
      [path setLineWidth:lineWidth];
      [[NSColor colorWithCalibratedWhite:1.0 alpha:0.5] set];
      [path stroke];
    }
  } else if (!inATabDraggingOverlayWindow_) {
    // If the window is not themed, and not being used to drag tabs between
    // browser windows, decrease the tab strip background's translucency by
    // overlaying it with a partially-transparent gray. The gray is somewhat
    // opaque for Incognito mode, very opaque for non-Incognito mode, and
    // completely opaque when the window is not active.
    if (const ui::ThemeProvider* themeProvider =
            [[self window] themeProvider]) {
      NSColor* overlayColor = nil;
      if (self.window.isMainWindow) {
        NSAppearance* appearance = self.effectiveAppearance;
        if ([appearance respondsToSelector:@selector(allowsVibrancy)] &&
            appearance.allowsVibrancy) {
          overlayColor = themeProvider->GetNSColor(
              ThemeProperties::COLOR_FRAME_VIBRANCY_OVERLAY);
        } else if (themeProvider->InIncognitoMode()) {
          overlayColor = [NSColor colorWithSRGBRed:20 / 255.
                                             green:22 / 255.
                                              blue:24 / 255.
                                             alpha:1];
        } else {
          overlayColor =
              themeProvider->GetNSColor(ThemeProperties::COLOR_FRAME);
        }
      } else {
        overlayColor =
            themeProvider->GetNSColor(ThemeProperties::COLOR_FRAME_INACTIVE);
      }

      if (overlayColor) {
        [overlayColor set];
        NSRectFillUsingOperation(dirtyRect, NSCompositeSourceOver);
      }
    }
  }
}
@end

@implementation TabStripBackgroundView {
  // Weak, owned by its superview.
  TabStripThemeBackgroundView* themeBackgroundView_;

  // Weak, owned by its superview.
  NSVisualEffectView* visualEffectView_;
}

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    themeBackgroundView_ = [[[TabStripThemeBackgroundView alloc]
        initWithFrame:self.bounds] autorelease];
    themeBackgroundView_.autoresizingMask =
        NSViewWidthSizable | NSViewHeightSizable;
    [self addSubview:themeBackgroundView_];
  }
  return self;
}

- (BOOL)inATabDraggingOverlayWindow {
  return themeBackgroundView_.inATabDraggingOverlayWindow;
}

- (void)setInATabDraggingOverlayWindow:(BOOL)inATabDraggingOverlayWindow {
  themeBackgroundView_.inATabDraggingOverlayWindow =
      inATabDraggingOverlayWindow;
}

- (void)updateVisualEffectView {
  const ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  const bool isFullscreen =
      (self.window.styleMask & NSFullScreenWindowMask) != 0;

  // Visual effects cause higher power consumption in full screen.
  if (!isFullscreen && themeProvider->UsingSystemTheme()) {
    if (!visualEffectView_) {
      visualEffectView_ =
          [[[NSVisualEffectView alloc] initWithFrame:self.bounds] autorelease];
      if (!visualEffectView_)
        return;

      visualEffectView_.autoresizingMask =
          NSViewWidthSizable | NSViewHeightSizable;
      visualEffectView_.appearance =
          [NSAppearance appearanceNamed:themeProvider->InIncognitoMode()
                                            ? NSAppearanceNameVibrantDark
                                            : NSAppearanceNameVibrantLight];
      [self addSubview:visualEffectView_];
      [visualEffectView_ addSubview:themeBackgroundView_];
    }
  } else {
    if (visualEffectView_) {
      [self addSubview:themeBackgroundView_];
      [visualEffectView_ removeFromSuperview];
      visualEffectView_ = nil;
    }
  }
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  // AppKit calls this method when the view's position in the view hierarchy
  // changes, even if its parent window won't change.
  if (newWindow == self.window)
    return;
  if (self.window)
    [self.window removeObserver:self forKeyPath:@"styleMask"];
  if (newWindow)
    [newWindow addObserver:self forKeyPath:@"styleMask" options:0 context:nil];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK_EQ(object, self.window);
  DCHECK([keyPath isEqualToString:@"styleMask"]);
  [self updateVisualEffectView];
}

// ThemedWindowDrawing implementation.

- (void)windowDidChangeTheme {
  [self updateVisualEffectView];
  [themeBackgroundView_ setNeedsDisplay:YES];
}

- (void)windowDidChangeActive {
  // TODO(sdy): It shouldn't be necessary to update the visual effect view
  // here, since it only changes when the theme changes), but the window isn't
  // associated with a themeProvider at init time.
  [self updateVisualEffectView];
  [themeBackgroundView_ setNeedsDisplay:YES];
}

@end
