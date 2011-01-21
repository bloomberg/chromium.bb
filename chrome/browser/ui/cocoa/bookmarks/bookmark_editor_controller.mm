// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_editor_controller.h"

#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "ui/base/l10n/l10n_util.h"

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
             configuration:(BookmarkEditor::Configuration)configuration {
  if ((self = [super initWithParentWindow:parentWindow
                                  nibName:@"BookmarkEditor"
                                  profile:profile
                                   parent:parent
                            configuration:configuration])) {
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
    [self setInitialName:base::SysUTF16ToNSString(node_->GetTitle())];
    std::string url_string = node_->GetURL().possibly_invalid_spec();
    initialUrl_.reset([[NSString stringWithUTF8String:url_string.c_str()]
                        retain]);
  } else {
    initialUrl_.reset([@"" retain]);
  }
  [self setDisplayURL:initialUrl_];
  [super awakeFromNib];
}

- (void)nodeRemoved:(const BookmarkNode*)node
         fromParent:(const BookmarkNode*)parent
{
  // Be conservative; it is needed (e.g. "Add Page...")
  node_ = NULL;
  [self cancel:self];
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
  if (okEnabled)
    [urlField_ setBackgroundColor:[NSColor whiteColor]];
  else
    [urlField_ setBackgroundColor:[NSColor colorWithCalibratedRed:1.0
                                                            green:0.67
                                                             blue:0.67
                                                            alpha:1.0]];
  return okEnabled;
}

// The the bookmark's URL is assumed to be valid (otherwise the OK button
// should not be enabled). Previously existing bookmarks for which the
// parent has not changed are updated in-place. Those for which the parent
// has changed are removed with a new node created under the new parent.
// Called by -[BookmarkEditorBaseController ok:].
- (NSNumber*)didCommit {
  NSString* name = [[self displayName] stringByTrimmingCharactersInSet:
                    [NSCharacterSet newlineCharacterSet]];
  string16 newTitle = base::SysNSStringToUTF16(name);
  const BookmarkNode* newParentNode = [self selectedNode];
  GURL newURL = [self GURLFromUrlField];
  if (!newURL.is_valid()) {
    // Shouldn't be reached -- OK button should be disabled if not valid!
    NOTREACHED();
    return [NSNumber numberWithBool:NO];
  }

  // Determine where the new/replacement bookmark is to go.
  BookmarkModel* model = [self bookmarkModel];
  // If there was an old node then we update the node, and move it to its new
  // parent if the parent has changed (rather than deleting it from the old
  // parent and adding to the new -- which also prevents the 'poofing' that
  // occurs when a node is deleted).
  if (node_) {
    model->SetURL(node_, newURL);
    model->SetTitle(node_, newTitle);
    const BookmarkNode* oldParentNode = [self parentNode];
    if (newParentNode != oldParentNode)
      model->Move(node_, newParentNode, newParentNode->GetChildCount());
  } else {
    // Otherwise, add a new bookmark at the end of the newly selected folder.
    model->AddURL(newParentNode, newParentNode->GetChildCount(), newTitle,
                  newURL);
  }
  return [NSNumber numberWithBool:YES];
}

- (NSColor *)urlFieldColor {
  return [urlField_ backgroundColor];
}

@end  // BookmarkEditorController

