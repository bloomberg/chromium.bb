// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_browser_window.h"

#include "base/logging.h"
#include "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "ui/base/theme_provider.h"

@implementation ChromeBrowserWindow

- (const ui::ThemeProvider*)themeProvider {
  id delegate = [self delegate];
  if (![delegate respondsToSelector:@selector(themeProvider)])
    return NULL;
  return [delegate themeProvider];
}

- (ThemedWindowStyle)themedWindowStyle {
  id delegate = [self delegate];
  if (![delegate respondsToSelector:@selector(themedWindowStyle)])
    return THEMED_NORMAL;
  return [delegate themedWindowStyle];
}

- (NSPoint)themeImagePositionForAlignment:(ThemeImageAlignment)alignment {
  id delegate = [self delegate];
  if (![delegate respondsToSelector:@selector(themeImagePositionForAlignment:)])
    return NSZeroPoint;
  return [delegate themeImagePositionForAlignment:alignment];
}

- (BOOL)inIncognitoMode {
  const ui::ThemeProvider* themeProvider = [self themeProvider];
  return themeProvider && themeProvider->InIncognitoMode();
}

- (BOOL)inIncognitoModeWithSystemTheme {
  const ui::ThemeProvider* themeProvider = [self themeProvider];
  return themeProvider && themeProvider->InIncognitoMode() &&
      themeProvider->UsingSystemTheme();
}

- (BOOL)hasDarkTheme {
  // If a system theme, return YES if Incognito.
  const ui::ThemeProvider* themeProvider = [self themeProvider];
  if (!themeProvider) {
    return NO;
  } else if (themeProvider->UsingSystemTheme()) {
    return themeProvider->InIncognitoMode();
  }

  // If the custom theme has a custom toolbar color, return YES if it's
  // dark.
  if (themeProvider->HasCustomColor(ThemeProperties::COLOR_TOOLBAR)) {
    NSColor* theColor =
        themeProvider->GetNSColor(ThemeProperties::COLOR_TOOLBAR);
    theColor =
        [theColor colorUsingColorSpaceName:NSCalibratedWhiteColorSpace];
    if (theColor != nil) {
      // The white componement cutoff is an empirical value.
      return [theColor whiteComponent] < 0.7;
    }
  }

  // If the custom theme has a custom tab text color, assume that a light
  // color means a dark tab background image, and therefore a dark theme.
  if (themeProvider->HasCustomColor(ThemeProperties::COLOR_TAB_TEXT)) {
    NSColor* theColor =
        themeProvider->GetNSColor(ThemeProperties::COLOR_TAB_TEXT);
    theColor =
        [theColor colorUsingColorSpaceName:NSCalibratedWhiteColorSpace];
    if (theColor != nil) {
      return [theColor whiteComponent] >= 0.7;
    }
  }

  return NO;
}

@end
