// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_EDITOR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_EDITOR_CONTROLLER_H_
#pragma once

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_editor_base_controller.h"

// A controller for the bookmark editor, opened by 1) Edit... from the
// context menu of a bookmark button, and 2) Bookmark this Page...'s Edit
// button.
@interface BookmarkEditorController : BookmarkEditorBaseController {
 @private
  const BookmarkNode* node_;  // weak; owned by the model
  scoped_nsobject<NSString> initialUrl_;
  NSString* displayURL_;  // Bound to a text field in the dialog.
  IBOutlet NSTextField* urlField_;
}

@property(nonatomic, copy) NSString* displayURL;

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
                      node:(const BookmarkNode*)node
             configuration:(BookmarkEditor::Configuration)configuration;

@end

@interface BookmarkEditorController (UnitTesting)
- (NSColor *)urlFieldColor;
@end

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_EDITOR_CONTROLLER_H_
