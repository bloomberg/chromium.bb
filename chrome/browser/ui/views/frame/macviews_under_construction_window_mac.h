// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MACVIEWS_UNDER_CONSTRUCTION_WINDOW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MACVIEWS_UNDER_CONSTRUCTION_WINDOW_MAC_H_

#import <AppKit/AppKit.h>

// MacViewsUnderConstructionWindow displays a striped yellow bar above a
// window. It's used by MacViews browser windows to make life easier for
// developers who occasionally forget to launch Chromium with the right flags,
// and to make it clear to external developers that MacViews browser windows
// aren't quite ready to ship. It can be deleted once Chrome uses MacViews
// browser windows by default.

@interface MacViewsUnderConstructionWindow : NSWindow
+ (void)attachToWindow:(NSWindow*)window;

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSWindowStyleMask)style
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)flag NS_UNAVAILABLE;
@end

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MACVIEWS_UNDER_CONSTRUCTION_WINDOW_MAC_H_
