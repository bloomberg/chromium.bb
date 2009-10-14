// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

@interface BookmarkBarView (Private)
- (void)themeDidChangeNotification:(NSNotification*)aNotification;
- (void)updateTheme:(GTMTheme*)theme;
@end

@implementation BookmarkBarView

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)awakeFromNib {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(themeDidChangeNotification:)
                        name:kGTMThemeDidChangeNotification
                      object:nil];
}

- (void)viewDidMoveToWindow {
  if ([self window])
    [self updateTheme:[self gtm_theme]];
}

- (void)themeDidChangeNotification:(NSNotification*)aNotification {
  GTMTheme* theme = [aNotification object];
  [self updateTheme:theme];
}

- (void)updateTheme:(GTMTheme*)theme {
  NSColor* color = [theme textColorForStyle:GTMThemeStyleBookmarksBarButton
                                      state:GTMThemeStateActiveWindow];
  [noItemTextfield_ setTextColor:color];
}

// Mouse down events on the bookmark bar should not allow dragging the parent
// window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

-(NSTextField*)noItemTextfield {
  return noItemTextfield_;
}

@end  // @implementation BookmarkBarView
