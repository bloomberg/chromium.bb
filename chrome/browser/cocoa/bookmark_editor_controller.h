// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_EDITOR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_EDITOR_CONTROLLER_H_

#import "chrome/browser/cocoa/bookmark_editor_base_controller.h"

// A controller for the bookmark editor, opened with Edit... from the
// context menu of a bookmark button.
@interface BookmarkEditorController : BookmarkEditorBaseController {
 @private
  const BookmarkNode* node_;  // weak; owned by the model
  scoped_nsobject<NSString> initialUrl_;
  NSString* displayURL_;  // Bound to a text field in the dialog.
}

@property (copy) NSString* displayURL;

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
                      node:(const BookmarkNode*)node
             configuration:(BookmarkEditor::Configuration)configuration
                   handler:(BookmarkEditor::Handler*)handler;

@end

#endif  /* CHROME_BROWSER_COCOA_BOOKMARK_EDITOR_CONTROLLER_H_ */
