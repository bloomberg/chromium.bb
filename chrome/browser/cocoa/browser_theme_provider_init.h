// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_THEME_PROVIDER_INIT_H_
#define CHROME_BROWSER_COCOA_BROWSER_THEME_PROVIDER_INIT_H_

#import "chrome/browser/cocoa/GTMTheme.h"

class BrowserThemeProvider;

// Provides a routine to initialize GTMTheme with data from a provided
// |BrowserThemeProvider|.
@interface GTMTheme(BrowserThemeProviderInitialization)
+ (GTMTheme*)themeWithBrowserThemeProvider:(BrowserThemeProvider*)provider
                            isOffTheRecord:(BOOL)offTheRecord;
@end

#endif  // CHROME_BROWSER_COCOA_BROWSER_THEME_PROVIDER_INIT_H_
