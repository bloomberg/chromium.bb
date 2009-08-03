// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autoseparating_menu.h"

@implementation AutoseparatingMenu

- (NSMenuItem *)insertItemWithTitle:(NSString *)aString
                             action:(SEL)aSelector
                      keyEquivalent:(NSString *)keyEquiv
                            atIndex:(NSInteger)index {
  if ([aString isEqualToString:@"-"]) {
    NSMenuItem* separator = [NSMenuItem separatorItem];
    [self insertItem:separator
             atIndex:index];
    return separator;
  } else {
    return [super insertItemWithTitle:aString
                               action:aSelector
                        keyEquivalent:keyEquiv
                              atIndex:index];
  }
}

@end
