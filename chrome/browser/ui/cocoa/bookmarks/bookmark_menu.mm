// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu.h"


@implementation BookmarkMenu

@synthesize id = id_;

// Convention in the bookmark bar controller: the bookmark button
// cells have a BookmarkNode as their represented object.  This object
// is placed in a BookmarkMenu at the time a cell is asked for its
// menu.
- (void)setRepresentedObject:(id)object {
  if ([object isKindOfClass:[NSNumber class]]) {
    id_ = static_cast<int64>([object longLongValue]);
  }
}

@end
