// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple custom NSView for the bookmark bar used to prevent clicking and
// dragging from moving the browser window.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_VIEW_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

@class BookmarkBarController;
@class BookmarkBarImportButton;
@class BookmarkBarItemContainer;
@class BookmarkBarTextField;

@interface BookmarkBarView : BackgroundGradientView {
 @private
  BOOL dropIndicatorShown_;
  CGFloat dropIndicatorPosition_;  // x position

  IBOutlet BookmarkBarController* controller_;
  IBOutlet BookmarkBarTextField* noItemTextfield_;
  IBOutlet BookmarkBarImportButton* importBookmarksButton_;
  BookmarkBarItemContainer* noItemContainer_;
}
- (BookmarkBarTextField*)noItemTextfield;
- (BookmarkBarImportButton*)importBookmarksButton;
- (BookmarkBarController*)controller;

@property(nonatomic, assign) IBOutlet BookmarkBarItemContainer* noItemContainer;
@end

@interface BookmarkBarView()  // TestingOrInternalAPI
@property(nonatomic, readonly) BOOL dropIndicatorShown;
@property(nonatomic, readonly) CGFloat dropIndicatorPosition;
- (void)setController:(id)controller;
@end


// NSTextField subclass responsible for routing -menu to the BookmarBarView.
// This is necessary when building with the 10.6 SDK because -rightMouseDown:
// does not follow the responder chain.
@interface BookmarkBarTextField : NSTextField {
 @private
  IBOutlet BookmarkBarView* barView_;
}
@end

// NSButton subclass responsible for routing -menu to the BookmarBarView.
// This is necessary when building with the 10.6 SDK because -rightMouseDown:
// does not follow the responder chain.
@interface BookmarkBarImportButton : NSButton {
 @private
  IBOutlet BookmarkBarView* barView_;
}
@end

// GTMWidthBasedTweaker subclass responsible for routing -menu to the
// BookmarBarView. This is necessary when building with the 10.6 SDK because
// -rightMouseDown: does not follow the responder chain.
@interface BookmarkBarItemContainer : GTMWidthBasedTweaker {
 @private
  IBOutlet BookmarkBarView* barView_;
}
@end

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_VIEW_H_
