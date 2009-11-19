// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"

@interface BookmarkEditorController (Private)

// Grab the url from the text field and convert.
- (GURL)GURLFromUrlField;

@end

@implementation BookmarkEditorController

@synthesize displayURL = displayURL_;

+ (NSSet*)keyPathsForValuesAffectingOkEnabled {
  return [NSSet setWithObject:@"displayURL"];
}

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
                      node:(const BookmarkNode*)node
             configuration:(BookmarkEditor::Configuration)configuration
                   handler:(BookmarkEditor::Handler*)handler {
  if ((self = [super initWithParentWindow:parentWindow
                                  nibName:@"BookmarkEditor"
                                  profile:profile
                                   parent:parent
                            configuration:configuration
                                  handler:handler])) {
    // "Add Page..." has no "node" so this may be NULL.
    node_ = node;
  }
  return self;
}

- (void)dealloc {
  [displayURL_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  // Set text fields to match our bookmark.  If the node is NULL we
  // arrived here from an "Add Page..." item in a context menu.
  if (node_) {
    [self setInitialName:base::SysWideToNSString(node_->GetTitle())];
    std::string url_string = node_->GetURL().possibly_invalid_spec();
    initialUrl_.reset([[NSString stringWithUTF8String:url_string.c_str()]
                        retain]);
  } else {
    initialUrl_.reset([@"" retain]);
  }
  [self setDisplayURL:initialUrl_];
  [super awakeFromNib];
}

#pragma mark Bookmark Editing

// If possible, return a valid GURL from the URL text field.
- (GURL)GURLFromUrlField {
  NSString* url = [self displayURL];
  GURL newURL = GURL([url UTF8String]);
  if (!newURL.is_valid()) {
    // Mimic observed friendliness from Windows
    newURL = GURL([[NSString stringWithFormat:@"http://%@", url] UTF8String]);
  }
  return newURL;
}

// Enable the OK button if there is a valid URL.
- (BOOL)okEnabled {
  BOOL okEnabled = NO;
  if ([[self displayURL] length]) {
    GURL newURL = [self GURLFromUrlField];
    okEnabled = (newURL.is_valid()) ? YES : NO;
  }
  return okEnabled;
}

// The the bookmark's URL is assumed to be valid (otherwise the OK button
// should not be enabled).  If the bookmark previously existed then it is
// removed from its old folder.  The bookmark is then added to its new
// folder.  If the folder has not changed then the bookmark stays in its
// original position (index) otherwise it is added to the end of the new
// folder.  Called by -[BookmarkEditorBaseController ok:].
- (NSNumber*)didCommit {
  NSString* name = [[self displayName] stringByTrimmingCharactersInSet:
                    [NSCharacterSet newlineCharacterSet]];
  std::wstring newTitle = base::SysNSStringToWide(name);
  const BookmarkNode* newParentNode = [self selectedNode];
  BookmarkModel* model = [self bookmarkModel];
  int newIndex = newParentNode->GetChildCount();
  GURL newURL = [self GURLFromUrlField];
  if (!newURL.is_valid()) {
    // Shouldn't be reached -- OK button should be disabled if not valid!
    NOTREACHED();
    return [NSNumber numberWithBool:NO];
  }

  // Determine where the new/replacement bookmark is to go.
  const BookmarkNode* parentNode = [self parentNode];
  if (node_ && parentNode) {
    // Replace the old bookmark with the updated bookmark.
    int oldIndex = parentNode->IndexOfChild(node_);
    if (oldIndex >= 0)
      model->Remove(parentNode, oldIndex);
    if (parentNode == newParentNode)
      newIndex = oldIndex;
  }
  // Add bookmark as new node at the end of the newly selected folder.
  const BookmarkNode* node = model->AddURL(newParentNode, newIndex,
                                           newTitle, newURL);
  // Honor handler semantics: callback on node creation.
  [self notifyHandlerCreatedNode:node];
  return [NSNumber numberWithBool:YES];
}

@end  // BookmarkEditorController

