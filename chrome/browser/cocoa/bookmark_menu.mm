// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_menu.h"


@implementation BookmarkMenu

@synthesize node = node_;

// Convention in the bookmark bar controller: the bookmark button
// cells have a BookmarkNode as their represented object.  This object
// is placed in a BookmarkMenu at the time a cell is asked for its
// menu.
- (void)setRepresentedObject:(id)object {
  if ([object isKindOfClass:[NSValue class]]) {
    node_ = static_cast<const BookmarkNode*>([object pointerValue]);
  }
}

@end
