// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/themed_browser_window.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "ui/base/theme_provider.h"

@implementation ThemedBrowserWindow 

- (ui::ThemeProvider*)themeProvider {
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

- (NSPoint)themePatternPhase {
  id delegate = [self delegate];
  if (![delegate respondsToSelector:@selector(themePatternPhase)])
    return NSMakePoint(0, 0);
  return [delegate themePatternPhase];
}

@end
