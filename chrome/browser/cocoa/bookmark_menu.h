// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
struct BookmarkNode;

// The context menu for bookmark buttons needs to know which
// BookmarkNode it is talking about.  For example, "Open All" is
// disabled if the bookmark node is a folder and has no children.
@interface BookmarkMenu : NSMenu {
 @private
  const BookmarkNode* node_;
}
- (void)setRepresentedObject:(id)object;
@property const BookmarkNode* node;
@end

