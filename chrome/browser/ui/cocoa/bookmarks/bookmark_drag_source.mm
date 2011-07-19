// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_drag_source.h"

#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents_view_mac.h"

@implementation BookmarkDragSource

- (id)initWithContentsView:(TabContentsViewCocoa*)contentsView
                  dropData:
                      (const std::vector<BookmarkNodeData::Element>&)dropData
                   profile:(Profile*)profile
                pasteboard:(NSPasteboard*)pboard
         dragOperationMask:(NSDragOperation)dragOperationMask {
  self = [super initWithContentsView:contentsView
                           pasteboard:pboard
                    dragOperationMask:dragOperationMask];
  if (self) {
    dropData_ = dropData;
    profile_ = profile;
  }

  return self;
}

- (void)fillPasteboard {
  bookmark_pasteboard_helper_mac::WriteToDragClipboard(dropData_,
      profile_->GetPath().value());
}

- (NSImage*)dragImage {
  // TODO(feldstein): Do something better than this. Should have badging
  // and a single drag image.
  // http://crbug.com/37264
  return [NSImage imageNamed:NSImageNameMultipleDocuments];
}

@end  // @implementation BookmarkDragSource

