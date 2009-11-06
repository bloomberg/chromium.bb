// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_EDITOR_BASE_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_EDITOR_BASE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"
#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"

@class BookmarkTreeBrowserCell;
class BookmarkModel;

// A base controller class for bookmark creation and editing dialogs which
// present the current bookmark folder structure in a tree view.  Do not
// instantiate this controller directly -- use one of its derived classes.
// NOTE: If your derived classes is intended to be dispatched via the
// BookmarkEditor::Show static function found in the accompanying
// implementation will need to update that function.
@interface BookmarkEditorBaseController :
    NSWindowController<NSMatrixDelegate, NSTextFieldDelegate> {
 @private
  IBOutlet NSBrowser* folderBrowser_;
  IBOutlet NSButton* newFolderButton_;
  IBOutlet NSButton* okButton_;  // Used for unit testing only.

  NSWindow* parentWindow_;
  Profile* profile_;  // weak
  const BookmarkNode* parentNode_;  // weak; owned by the model
  BookmarkEditor::Configuration configuration_;
  scoped_ptr<BookmarkEditor::Handler> handler_;  // we take ownership
  NSString* initialName_;
  NSString* displayName_;  // Bound to a text field in the dialog.
  BOOL okEnabled_;  // Bound to the OK button.
}

@property (copy) NSString* initialName;
@property (copy) NSString* displayName;
@property (assign) BOOL okEnabled;

// Designated initializer.  Derived classes should call through to this init.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                   nibName:(NSString*)nibName
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
             configuration:(BookmarkEditor::Configuration)configuration
                   handler:(BookmarkEditor::Handler*)handler;

// Run the bookmark editor as a modal sheet.  Does not block.
- (void)runAsModalSheet;

// Create a new folder at the end of the selected parent folder, give it
// an untitled name, and put it into editing mode.
- (IBAction)newFolder:(id)sender;

// Actions for the buttons at the bottom of the window.
- (IBAction)cancel:(id)sender;

// The OK action will record the edited bookmark.  The default dismisses
// the dialog and should be called by the derived class if overridden.
- (IBAction)ok:(id)sender;

// Methods for use by derived classes only.

// Determine and returns the rightmost selected/highlighted element (node)
// in the bookmark tree view if the tree view is showing, otherwise returns
// the original |parentNode_|.  If the tree view is showing but nothing is
// selected then return the root node.  This assumes that leaf nodes (pure
// bookmarks) are not being presented.
- (const BookmarkNode*)selectedNode;

// Select/highlight the given node within the browser tree view.  If the
// node is nil then select the bookmark bar node.  Exposed for unit test.
- (void)selectNodeInBrowser:(const BookmarkNode*)node;

// Notify the handler, if any, of a node creation.
- (void)NotifyHandlerCreatedNode:(const BookmarkNode*)node;

// Accessors
- (BookmarkModel*)bookmarkModel;
- (const BookmarkNode*)parentNode;

@end

@interface BookmarkEditorBaseController(TestingAPI)
@property (readonly) BOOL okButtonEnabled;
- (void)selectTestNodeInBrowser:(const BookmarkNode*)node;
+ (const BookmarkNode*)folderChildForParent:(const BookmarkNode*)parent
                                  withFolderIndex:(NSInteger)index;
+ (int)indexOfFolderChild:(const BookmarkNode*)child;
@end

#endif  /* CHROME_BROWSER_COCOA_BOOKMARK_EDITOR_BASE_CONTROLLER_H_ */
