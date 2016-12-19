// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#include <set>

@class BookmarkFolderViewController;
namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

@protocol BookmarkFolderViewControllerDelegate
// Called when a bookmark folder is selected. |folder| is the newly selected
// folder.
- (void)folderPicker:(BookmarkFolderViewController*)folderPicker
    didFinishWithFolder:(const bookmarks::BookmarkNode*)folder;
// Called when the user is done with the picker, either by tapping the Cancel or
// the Back button.
- (void)folderPickerDidCancel:(BookmarkFolderViewController*)folderPicker;
@end

// A folder selector view controller.
//
// This controller monitors the state of the bookmark model, so changes to the
// bookmark model can affect this controller's state.
// The bookmark model is assumed to be loaded, thus also not to be NULL.
@interface BookmarkFolderViewController : UIViewController

@property(nonatomic, assign) id<BookmarkFolderViewControllerDelegate> delegate;

// The current nodes (bookmarks or folders) that are considered for a move.
@property(nonatomic, assign, readonly)
    const std::set<const bookmarks::BookmarkNode*>& editedNodes;

// Initializes the view controller with a bookmarks model. |allowsNewFolders|
// will instruct the controller to provide the necessary UI to create a folder.
// |bookmarkModel| must not be NULL and must be loaded.
// |editedNodes| affects which cells can be selected, since it is not possible
// to move a node into its subnode.
// |allowsCancel| puts a cancel and done button in the navigation bar instead of
// a back button, which is needed if this view controller is presented modally.
- (instancetype)
initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
     allowsNewFolders:(BOOL)allowsNewFolders
          editedNodes:(const std::set<const bookmarks::BookmarkNode*>&)nodes
         allowsCancel:(BOOL)allowsCancel
       selectedFolder:(const bookmarks::BookmarkNode*)selectedFolder;

// This method changes the currently selected folder and updates the UI. The
// delegate is not notified of the change.
- (void)changeSelectedFolder:(const bookmarks::BookmarkNode*)selectedFolder;

#pragma mark UIScrollViewDelegate

// Updates the MDCAppBar with changes to the collection view scroll state. Must
// be called by subclasses if they override this method in order to maintain
// this functionality.
- (void)scrollViewDidScroll:(UIScrollView*)scrollView NS_REQUIRES_SUPER;

// Updates the MDCAppBar with changes to the collection view scroll state. Must
// be called by subclasses if they override this method in order to maintain
// this functionality.
- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate NS_REQUIRES_SUPER;

// Updates the MDCAppBar with changes to the collection view scroll state. Must
// be called by subclasses if they override this method in order to maintain
// this functionality.
- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView
    NS_REQUIRES_SUPER;

// Updates the MDCAppBar with changes to the collection view scroll state. Must
// be called by subclasses if they override this method in order to maintain
// this functionality.
- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset
    NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_VIEW_CONTROLLER_H_
