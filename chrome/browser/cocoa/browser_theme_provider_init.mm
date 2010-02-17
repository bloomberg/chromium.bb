// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/browser_theme_provider_init.h"

#import <Cocoa/Cocoa.h>

#include <map>
#include <string>
#include <utility>

#import "base/scoped_nsobject.h"
#include "chrome/browser/browser_theme_provider.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"


@implementation GTMTheme(BrowserThemeProviderInitialization)

+ (GTMTheme*)themeWithBrowserThemeProvider:(BrowserThemeProvider*)provider
                            isOffTheRecord:(BOOL)isOffTheRecord {
  // First check if it's in the cache.
  typedef std::pair<std::string, BOOL> ThemeKey;
  static std::map<ThemeKey, GTMTheme*> cache;
  ThemeKey key(provider->GetThemeID(), isOffTheRecord);
  GTMTheme* theme = cache[key];
  if (theme)
    return theme;

  theme = [[GTMTheme alloc] init];  // "Leak" it in the cache.
  cache[key] = theme;

  // TODO(pinkerton): Need to be able to theme the entire incognito window
  // http://crbug.com/18568 The hardcoding of the colors here will need to be
  // removed when that bug is addressed, but are here in order for things to be
  // usable in the meantime.
  if (isOffTheRecord) {
    NSColor* incognitoColor = [NSColor colorWithCalibratedRed:83/255.0
                                                        green:108.0/255.0
                                                         blue:140/255.0
                                                        alpha:1.0];
    [theme setBackgroundColor:incognitoColor];
    [theme setValue:[NSColor blackColor]
       forAttribute:@"textColor"
              style:GTMThemeStyleTabBarSelected
              state:GTMThemeStateActiveWindow];
    [theme setValue:[NSColor blackColor]
       forAttribute:@"textColor"
              style:GTMThemeStyleTabBarDeselected
              state:GTMThemeStateActiveWindow];
    [theme setValue:[NSColor blackColor]
       forAttribute:@"textColor"
              style:GTMThemeStyleBookmarksBarButton
              state:GTMThemeStateActiveWindow];
    [theme setValue:[NSColor blackColor]
       forAttribute:@"iconColor"
              style:GTMThemeStyleToolBarButton
              state:GTMThemeStateActiveWindow];
    return theme;
  }

  NSImage* frameImage = provider->HasCustomImage(IDR_THEME_FRAME) ?
      provider->GetNSImageNamed(IDR_THEME_FRAME) : nil;
  if (frameImage) {
    NSImage* frameInactiveImage =
        provider->GetNSImageNamed(IDR_THEME_FRAME_INACTIVE);
    [theme setValue:frameImage
       forAttribute:@"backgroundImage"
              style:GTMThemeStyleWindow
              state:GTMThemeStateActiveWindow];
    [theme setValue:frameInactiveImage
       forAttribute:@"backgroundImage"
              style:GTMThemeStyleWindow
              state:0];
  }

  NSColor* tabTextColor =
      provider->GetNSColor(BrowserThemeProvider::COLOR_TAB_TEXT);
  [theme setValue:tabTextColor
     forAttribute:@"textColor"
            style:GTMThemeStyleTabBarSelected
            state:GTMThemeStateActiveWindow];

  NSColor* tabInactiveTextColor =
      provider->GetNSColor(BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT);
  [theme setValue:tabInactiveTextColor
     forAttribute:@"textColor"
            style:GTMThemeStyleTabBarDeselected
            state:GTMThemeStateActiveWindow];

  NSColor* bookmarkBarTextColor =
      provider->GetNSColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
  [theme setValue:bookmarkBarTextColor
     forAttribute:@"textColor"
            style:GTMThemeStyleBookmarksBarButton
            state:GTMThemeStateActiveWindow];


  NSImage* toolbarImage = provider->HasCustomImage(IDR_THEME_TOOLBAR) ?
      provider->GetNSImageNamed(IDR_THEME_TOOLBAR) : nil;
  [theme setValue:toolbarImage
     forAttribute:@"backgroundImage"
            style:GTMThemeStyleToolBar
            state:GTMThemeStateActiveWindow];
  NSImage* toolbarBackgroundImage =
      provider->HasCustomImage(IDR_THEME_TAB_BACKGROUND) ?
          provider->GetNSImageNamed(IDR_THEME_TAB_BACKGROUND) : nil;
  [theme setValue:toolbarBackgroundImage
     forAttribute:@"backgroundImage"
            style:GTMThemeStyleTabBarDeselected
            state:GTMThemeStateActiveWindow];

  NSImage* toolbarButtonImage =
      provider->HasCustomImage(IDR_THEME_BUTTON_BACKGROUND) ?
          provider->GetNSImageNamed(IDR_THEME_BUTTON_BACKGROUND) : nil;
  if (toolbarButtonImage) {
    [theme setValue:toolbarButtonImage
       forAttribute:@"backgroundImage"
              style:GTMThemeStyleToolBarButton
              state:GTMThemeStateActiveWindow];
  } else {
    NSColor* startColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.0];
    NSColor* endColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.3];
    scoped_nsobject<NSGradient> gradient([[NSGradient alloc]
                                          initWithStartingColor:startColor
                                                    endingColor:endColor]);

    [theme setValue:gradient
       forAttribute:@"gradient"
              style:GTMThemeStyleToolBarButton
              state:GTMThemeStateActiveWindow];

    [theme setValue:gradient
       forAttribute:@"gradient"
              style:GTMThemeStyleToolBarButton
              state:GTMThemeStateActiveWindow];
  }

  NSColor* toolbarButtonIconColor =
      provider->GetNSColorTint(BrowserThemeProvider::TINT_BUTTONS);
  [theme setValue:toolbarButtonIconColor
     forAttribute:@"iconColor"
            style:GTMThemeStyleToolBarButton
            state:GTMThemeStateActiveWindow];

  NSColor* toolbarButtonBorderColor = toolbarButtonIconColor;
  [theme setValue:toolbarButtonBorderColor
     forAttribute:@"borderColor"
            style:GTMThemeStyleToolBar
            state:GTMThemeStateActiveWindow];

  NSColor* toolbarBackgroundColor =
      provider->GetNSColor(BrowserThemeProvider::COLOR_TOOLBAR);
  [theme setValue:toolbarBackgroundColor
     forAttribute:@"backgroundColor"
            style:GTMThemeStyleToolBar
            state:GTMThemeStateActiveWindow];

  NSImage* frameOverlayImage =
      provider->HasCustomImage(IDR_THEME_FRAME_OVERLAY) ?
          provider->GetNSImageNamed(IDR_THEME_FRAME_OVERLAY) : nil;
  if (frameOverlayImage) {
    [theme setValue:frameOverlayImage
       forAttribute:@"overlay"
              style:GTMThemeStyleWindow
              state:GTMThemeStateActiveWindow];
    NSImage* frameOverlayInactiveImage =
        provider->GetNSImageNamed(IDR_THEME_FRAME_OVERLAY_INACTIVE);
    if (frameOverlayInactiveImage) {
      [theme setValue:frameOverlayInactiveImage
         forAttribute:@"overlay"
                style:GTMThemeStyleWindow
                state:GTMThemeStateInactiveWindow];
    }
  }

  return theme;
}


@end  // @implementation GTMTheme(BrowserThemeProviderInitialization)
