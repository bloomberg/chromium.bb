// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bubble_view.h"

#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

// The roundedness of the edges of our bubble.
const int kBubbleCornerRadius = 4.0f;
const float kWindowEdge = 0.7f;

@implementation BubbleView

// Designated initializer. |provider| is the window from which we get the
// current theme to draw text and backgrounds. If nil, the current window will
// be checked. Defaults to all corners being rounded. The caller needs to
// ensure |provider| can't go away as it will not be retained.
- (id)initWithFrame:(NSRect)frame themeProvider:(NSWindow*)provider {
  if ((self = [super initWithFrame:frame])) {
    cornerFlags_ = kRoundedAllCorners;
    themeProvider_ = provider;
  }
  return self;
}

// Sets the string displayed in the bubble. A copy of the string is made.
- (void)setContent:(NSString*)content {
  content_.reset([content copy]);
  [self setNeedsDisplay:YES];
}

// Sets which corners will be rounded.
- (void)setCornerFlags:(unsigned long)flags {
  cornerFlags_ = flags;
  [self setNeedsDisplay:YES];
}

- (NSString*)content {
  return content_.get();
}

- (unsigned long)cornerFlags {
  return cornerFlags_;
}

// The font used to display the content string.
- (NSFont*)font {
  return [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
}

// Asks the given theme provider for its theme. If there isn't one specified,
// check the window we are in. May still return nil if the window doesn't
// support themeing.
- (GTMTheme*)gtm_theme {
  GTMTheme* theme = [themeProvider_ gtm_theme];
  if (!theme)
    theme = [[self window] gtm_theme];
  return theme;
}

// Draws the themed background and the text. Will draw a gray bg if no theme.
- (void)drawRect:(NSRect)rect {
  float topLeftRadius =
      cornerFlags_ & kRoundedTopLeftCorner ? kBubbleCornerRadius : 0;
  float topRightRadius =
      cornerFlags_ & kRoundedTopRightCorner ? kBubbleCornerRadius : 0;
  float bottomLeftRadius =
      cornerFlags_ & kRoundedBottomLeftCorner ? kBubbleCornerRadius : 0;
  float bottomRightRadius =
      cornerFlags_ & kRoundedBottomRightCorner ? kBubbleCornerRadius : 0;

  GTMTheme* theme = [self gtm_theme];

  // Background / Edge

  NSRect bounds = [self bounds];
  bounds = NSInsetRect(bounds, 0.5, 0.5);
  NSBezierPath* border =
      [NSBezierPath gtm_bezierPathWithRoundRect:bounds
                            topLeftCornerRadius:topLeftRadius
                           topRightCornerRadius:topRightRadius
                         bottomLeftCornerRadius:bottomLeftRadius
                        bottomRightCornerRadius:bottomRightRadius];

  NSColor* color =
      [theme backgroundColorForStyle:GTMThemeStyleToolBar
                               state:GTMThemeStateActiveWindow];

  // workaround for default theme
  // TODO(alcor) next GTM update return nil for background color if not set;
  // http://crbug.com/25196
  if ([color isEqual:[NSColor colorWithCalibratedWhite:0.5 alpha:1.0]])
    color = nil;
  if (!color)
    color = [NSColor colorWithCalibratedWhite:0.9 alpha:1.0];
  [color set];
  [border fill];

  [[NSColor colorWithDeviceWhite:kWindowEdge alpha:1.0f] set];
  [border stroke];

  // Text
  NSColor* textColor = [theme textColorForStyle:GTMThemeStyleTabBarSelected
                                          state:GTMThemeStateActiveWindow];
  NSFont* textFont = [self font];
  scoped_nsobject<NSShadow> textShadow([[NSShadow alloc] init]);
  [textShadow setShadowBlurRadius:0.0f];
  [textShadow.get() setShadowColor:[textColor gtm_legibleTextColor]];
  [textShadow.get() setShadowOffset:NSMakeSize(0.0f, -1.0f)];

  NSDictionary* textDict = [NSDictionary dictionaryWithObjectsAndKeys:
      textColor, NSForegroundColorAttributeName,
      textFont, NSFontAttributeName,
      textShadow.get(), NSShadowAttributeName,
      nil];
  [content_ drawAtPoint:NSMakePoint(kBubbleViewTextPositionX,
                                    kBubbleViewTextPositionY)
         withAttributes:textDict];
}

@end
