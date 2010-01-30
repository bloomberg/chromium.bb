// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/draggable_button.h"

@protocol BookmarkButtonDelegate;

// Class for bookmark bar buttons that can be drag sources.
@interface BookmarkButton : DraggableButton {
 @private
  id<BookmarkButtonDelegate> delegate_;
}

@property(assign, nonatomic) id<BookmarkButtonDelegate> delegate;

@end  // @interface BookmarkButton

// Protocol for a |BookmarkButton|'s delegate, which is responsible for doing
// things which require information about the bookmark represented by this
// button.
@protocol BookmarkButtonDelegate

// Fill the given pasteboard with appropriate data when the given button is
// dragged. Since the delegate has no way of providing pasteboard data later,
// all data must actually be put into the pasteboard and not merely promised.
- (void)fillPasteboard:(NSPasteboard*)pboard
       forDragOfButton:(BookmarkButton*)button;

@end  // @protocol BookmarkButtonDelegate
