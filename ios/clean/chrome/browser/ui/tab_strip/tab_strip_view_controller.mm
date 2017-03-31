// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_strip/tab_strip_view_controller.h"

#import "ios/clean/chrome/browser/ui/commands/tab_strip_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CGFloat kItemLength = 100.0f;
CGFloat kSpacing = 10.0f;
}

@implementation TabStripViewController
@synthesize dispatcher = _dispatcher;

- (UICollectionViewLayout*)collectionViewLayout {
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.itemSize = CGSizeMake(kItemLength, kItemLength);
  layout.minimumInteritemSpacing = kSpacing;
  layout.sectionInset =
      UIEdgeInsetsMake(kSpacing, kSpacing, kSpacing, kSpacing);
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  return layout;
}

- (void)showTabAtIndex:(int)index {
  [self.dispatcher showTabStripTabAtIndex:index];
}

- (void)closeTabAtIndex:(int)index {
  [self.dispatcher closeTabStripTabAtIndex:index];
}

@end
