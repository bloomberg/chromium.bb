// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"

@protocol BookmarkButtonControllerProtocol;
@class BookmarkBarFolderController;

// Main content view for a bookmark bar folder "menu" window.  This is
// logically similar to a BookmarkBarView but is oriented vertically.
@interface BookmarkBarFolderView : NSView {
 @private
  BOOL inDrag_;  // Are we in the middle of a drag?
  BOOL dropIndicatorShown_;
  // The following |controller_| is weak; used for testing only. See the imple-
  // mentation comment for - (id<BookmarkButtonControllerProtocol>)controller.
  id<BookmarkButtonControllerProtocol> controller_;
  base::scoped_nsobject<NSBox> dropIndicator_;
}
@end
