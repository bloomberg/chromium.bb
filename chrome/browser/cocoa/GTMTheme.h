// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Derived from the GTMTheme in Google Toolbox for Mac.  This file should
// go away: http://crbug.com/35554

#import "GTMDefines.h"
#import <AppKit/AppKit.h>

// Sent whenever the theme changes. Object => GTMTheme that changed
GTM_EXTERN NSString *const kGTMThemeDidChangeNotification;

// Key for user defaults defining background color
GTM_EXTERN NSString *const kGTMThemeBackgroundColorKey;

enum {
  GTMThemeStyleTabBarSelected,
  GTMThemeStyleTabBarDeselected,
  GTMThemeStyleWindow,
  GTMThemeStyleToolBar,
  GTMThemeStyleToolBarButton,
  GTMThemeStyleToolBarButtonPressed,
  GTMThemeStyleBookmarksBarButton,
};
typedef NSUInteger GTMThemeStyle;

enum {
  GTMThemeStateInactiveWindow = 0,
  GTMThemeStateActiveWindow = 1 << 0
};
typedef NSUInteger GTMThemeState;

// GTMTheme provides a range of values for procedural drawing of UI elements
// based on interpolation of a single background color

@interface GTMTheme : NSObject {
 @private
  NSColor *backgroundColor_;  // bound to user defaults
  NSImage *backgroundImage_;  // bound to user defaults
  NSMutableDictionary *values_; // cached values
}

// Access the global theme. By default this is bound to user defaults
+ (GTMTheme *)defaultTheme;
+ (void)setDefaultTheme:(GTMTheme *)theme;

// Bind this theme to user defaults
- (void)bindToUserDefaults;

// returns base theme color
- (NSColor *)backgroundColor;

// sets the base theme color
- (void)setBackgroundColor:(NSColor *)value;

// base background image
- (NSImage *)backgroundImage;

// set base background image
- (void)setBackgroundImage:(NSImage *)value;

// NSImage pattern background
- (NSImage *)backgroundImageForStyle:(GTMThemeStyle)style
                               state:(GTMThemeState)state;

// NSColor of the above image, if present, else gradientForStyle
- (NSColor *)backgroundPatternColorForStyle:(GTMThemeStyle)style
                                      state:(GTMThemeState)state;

// NSGradient for specific usage
- (NSGradient *)gradientForStyle:(GTMThemeStyle)style
                           state:(GTMThemeState)state;

// Outline color for stroke
- (NSColor *)strokeColorForStyle:(GTMThemeStyle)style
                           state:(GTMThemeState)state;

// Text color
- (NSColor *)textColorForStyle:(GTMThemeStyle)style
                         state:(GTMThemeState)state;

// Base background color (a plain (non pattern/gradient) NSColor)
- (NSColor *)backgroundColorForStyle:(GTMThemeStyle)style
                               state:(GTMThemeState)state;

// Indicates whether luminance is dark or light
- (BOOL)styleIsDark:(GTMThemeStyle)style state:(GTMThemeState)state;

// Background style for this style and state
- (NSBackgroundStyle)interiorBackgroundStyleForStyle:(GTMThemeStyle)style
                                               state:(GTMThemeState)state;

- (NSColor *)iconColorForStyle:(GTMThemeStyle)style
                         state:(GTMThemeState)state;

// Manually set a theme value
- (void)setValue:(id)value
    forAttribute:(NSString *)attribute
           style:(GTMThemeStyle)style
           state:(GTMThemeState)state;

// Manually extract a theme value
- (id)valueForAttribute:(NSString *)attribute
                  style:(GTMThemeStyle)style
                  state:(GTMThemeState)state;
@end

// Convenience categories for NSWindow and NSView to access the current theme
// Default implementation polls the window delegate
@interface NSWindow (GTMTheme)
- (GTMTheme *)gtm_theme;
- (NSPoint)gtm_themePatternPhase;
@end

@interface NSView (GTMTheme)
- (GTMTheme *)gtm_theme;
- (NSPoint)gtm_themePatternPhase;
@end

@protocol GTMThemeDelegate
- (GTMTheme *)gtm_themeForWindow:(NSWindow *)window;
- (NSPoint)gtm_themePatternPhaseForWindow:(NSWindow *)window;
@end
