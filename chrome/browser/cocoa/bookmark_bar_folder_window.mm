// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_folder_window.h"

#import "chrome/browser/cocoa/bookmark_bar_folder_controller.h"
#import "chrome/browser/cocoa/themed_window.h"

@implementation BookmarkBarFolderWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  return [super initWithContentRect:contentRect
                          styleMask:NSBorderlessWindowMask // override
                            backing:bufferingType
                              defer:deferCreation];
}

- (ThemeProvider*)themeProvider {
  return [[self windowController] themeProvider];
}

@end
