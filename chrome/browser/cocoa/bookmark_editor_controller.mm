// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profile.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"

@interface BookmarkEditorController (Private)

// Grab the url from the text field and convert.
- (GURL)GURLFromUrlField;

// Determine and returns the rightmost selected/highlighted element (node)
// in the bookmark tree view if the tree view is showing, otherwise returns
// the original parentNode_.  If the tree view is showing but nothing is
// selected then return the root node.
- (const BookmarkNode*)selectedParentNode;

// Select/highlight the given node within the browser tree view.  If the
// node is not found then the selection will not be changed.
- (void)selectNodeInBrowser:(const BookmarkNode*)node;

@end

// static; implemented for each platform.
void BookmarkEditor::Show(gfx::NativeWindow parent_hwnd,
                          Profile* profile,
                          const BookmarkNode* parent,
                          const EditDetails& details,
                          Configuration configuration,
                          Handler* handler) {
  if (details.type == EditDetails::NEW_FOLDER) {
    // TODO(sky): implement this.
    NOTIMPLEMENTED();
    return;
  }
  BookmarkEditorController* controller = [[BookmarkEditorController alloc]
      initWithParentWindow:parent_hwnd
                   profile:profile
                    parent:parent
                      node:details.existing_node
             configuration:configuration
                   handler:handler];
  [controller runAsModalSheet];
}

#pragma mark Bookmark TreeNode Helpers

namespace {

// Find the index'th folder child of a parent, ignoring bookmarks (leafs).
const BookmarkNode* GetFolderChildForParent(const BookmarkNode* parent_node,
                                            NSInteger folder_index) {
  const BookmarkNode* child_node = nil;
  int i = 0;
  int child_count = parent_node->GetChildCount();
  do {
    child_node = parent_node->GetChild(i);
    if (child_node->type() != BookmarkNode::URL)
      --folder_index;
    ++i;
  } while (folder_index >= 0 && i < child_count);
  return child_node;
}

// Determine the index of a child within its parent ignoring
// bookmarks (leafs).
int IndexOfFolderChild(const BookmarkNode* child_node) {
  const BookmarkNode* node_parent = child_node->GetParent();
  int child_index = node_parent->IndexOfChild(child_node);
  for (int i = child_index - 1; i >= 0; --i) {
    const BookmarkNode* sibling = node_parent->GetChild(i);
    if (sibling->type() == BookmarkNode::URL)
      --child_index;
  }
  return child_index;
}

} // namespace

@implementation BookmarkEditorController

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
                      node:(const BookmarkNode*)node
             configuration:(BookmarkEditor::Configuration)configuration
                   handler:(BookmarkEditor::Handler*)handler {
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"BookmarkEditor"
                        ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    parentWindow_ = parentWindow;
    profile_ = profile;
    parentNode_ = parent;
    // "Add Page..." has no "node" so this may be NULL.
    node_ = node;
    configuration_ = configuration;
    handler_.reset(handler);
  }
  return self;
}

- (void)awakeFromNib {
  // Set text fields to match our bookmark.  If the node is NULL we
  // arrived here from an "Add Page..." item in a context menu.
  if (node_) {
    initialName_.reset([base::SysWideToNSString(node_->GetTitle()) retain]);
    std::string url_string = node_->GetURL().possibly_invalid_spec();
    initialUrl_.reset([[NSString stringWithUTF8String:url_string.c_str()]
                        retain]);
  } else {
    initialName_.reset([@"" retain]);
    initialUrl_.reset([@"" retain]);
  }
  [nameField_ setStringValue:initialName_];
  [urlField_ setStringValue:initialUrl_];

  // Get a ping when the URL or name text fields change;
  // trigger an initial ping to set things up.
  [nameField_ setDelegate:self];
  [urlField_ setDelegate:self];
  [self controlTextDidChange:nil];

  if (configuration_ != BookmarkEditor::SHOW_TREE) {
    // Remember the NSBrowser's height; we will shrink our frame by that
    // much.
    NSRect frame = [[self window] frame];
    CGFloat browserHeight = [browser_ frame].size.height;
    frame.size.height -= browserHeight;
    frame.origin.y += browserHeight;
    // Remove the NSBrowser and "new folder" button.
    [browser_ removeFromSuperview];
    [newFolderButton_ removeFromSuperview];
    // Finally, commit the size change.
    [[self window] setFrame:frame display:YES];
  }
}

- (void)selectNodeInBrowser:(const BookmarkNode*)node {
  DCHECK(configuration_ == BookmarkEditor::SHOW_TREE);
  // Find and select the node in the browser by walking
  // back to the root node, then selecting forward.
  std::deque<NSInteger> rowsToSelect;
  const BookmarkNode* nodeParent = node->GetParent();
  // There should always be a parent node.
  DCHECK(nodeParent);
  while (nodeParent) {
    int nodeRow = IndexOfFolderChild(node);
    rowsToSelect.push_front(nodeRow);
    node = nodeParent;
    nodeParent = nodeParent->GetParent();
  }
  for (std::deque<NSInteger>::size_type column = 0;
       column < rowsToSelect.size();
       ++column) {
    [browser_ selectRow:rowsToSelect[column] inColumn:column];
  }
  [self controlTextDidChange:nil];
}

- (void)windowDidLoad {
  if (configuration_ == BookmarkEditor::SHOW_TREE) {
    // Find and select the |parent| bookmark node.
    [self selectNodeInBrowser:parentNode_];
  }
}

/* TODO(jrg):
// Implementing this informal protocol allows us to open the sheet
// somewhere other than at the top of the window.  NOTE: this means
// that I, the controller, am also the window's delegate.
- (NSRect)window:(NSWindow*)window willPositionSheet:(NSWindow*)sheet
        usingRect:(NSRect)rect {
  // adjust rect.origin.y to be the bottom of the toolbar
  return rect;
}
*/

// TODO(jrg): consider NSModalSession.
- (void)runAsModalSheet {
  [NSApp beginSheet:[self window]
     modalForWindow:parentWindow_
      modalDelegate:self
     didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
        contextInfo:nil];
}

// TODO(jrg)
- (IBAction)newFolder:(id)sender {
  NOTIMPLEMENTED();
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window]];
}

// If possible, return a valid GURL from the URL text field.
- (GURL)GURLFromUrlField {
  NSString* url = [urlField_ stringValue];
  GURL newURL = GURL([url UTF8String]);
  if (!newURL.is_valid()) {
    // Mimic observed friendliness from Windows
    newURL = GURL([[NSString stringWithFormat:@"http://%@", url] UTF8String]);
  }
  return newURL;
}

- (const BookmarkNode*)selectedParentNode {
  BookmarkModel* model = profile_->GetBookmarkModel();
  const BookmarkNode* selectedParentNode = NULL;
  // Determine a new parent node only if the browser is showing.
  if (configuration_ == BookmarkEditor::SHOW_TREE) {
    selectedParentNode = model->root_node();
    NSInteger column = 0;
    NSInteger selectedRow = [browser_ selectedRowInColumn:column];
    while (selectedRow >= 0) {
      selectedParentNode = GetFolderChildForParent(selectedParentNode,
                                                   selectedRow);
      ++column;
      selectedRow = [browser_ selectedRowInColumn:column];
    }
  } else {
    // If the tree is not showing then we use the original parent.
    selectedParentNode = parentNode_;
  }
  return selectedParentNode;
}

// Enable the OK button if there is a bookmark name and there is a valid URL.
// We set ourselves as the delegate of urlField_ so this gets called.
// (Yes, setting ourself as a delegate automatically registers us for
// the notification.)
- (void)controlTextDidChange:(NSNotification*)aNotification {
  // Name must not be empty, but it can be whitespace.
  NSString* name = [nameField_ stringValue];
  GURL newURL = [self GURLFromUrlField];
  [okButton_ setEnabled:([name length] != 0 && newURL.is_valid()) ? YES : NO];
}

// The ok: action is connected to the OK button in the Edit Bookmark window
// of the BookmarkEditor.xib.  The the bookmark's name and URL are assumed
// to be valid (otherwise the OK button should not be enabled).  If the
// bookmark previously existed then it is removed from its old folder.
// The bookmark is then added to its new folder.  If the folder has not
// changed then the bookmark stays in its original position (index) otherwise
// it is added to the end of the new folder.
- (IBAction)ok:(id)sender {
  NSString* name = [nameField_ stringValue];
  std::wstring newTitle = base::SysNSStringToWide(name);
  GURL newURL = [self GURLFromUrlField];
  if (!newURL.is_valid()) {
    // Shouldn't be reached -- OK button disabled if not valid!
    NOTREACHED();
    return;
  }

  // Determine where the new/replacement bookmark is to go.
  const BookmarkNode* newParentNode = [self selectedParentNode];
  BookmarkModel* model = profile_->GetBookmarkModel();
  int newIndex = newParentNode->GetChildCount();
  if (node_ && parentNode_) {
    // Replace the old bookmark with the updated bookmark.
    int oldIndex = parentNode_->IndexOfChild(node_);
    if (oldIndex >= 0)
      model->Remove(parentNode_, oldIndex);
    if (parentNode_ == newParentNode)
      newIndex = oldIndex;
  }
  // Add bookmark as new node at the end of the newly selected folder.
  const BookmarkNode* node = model->AddURL(newParentNode, newIndex,
                                           newTitle, newURL);
  // Honor handler semantics: callback on node creation.
  if (handler_.get())
    handler_->NodeCreated(node);
  [NSApp endSheet:[self window]];
}

- (void)didEndSheet:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  // This is probably unnecessary but it feels cleaner since the
  // delegate of a text field can be automatically registered for
  // notifications.
  [nameField_ setDelegate:nil];
  [urlField_ setDelegate:nil];

  [[self window] orderOut:self];

  // BookmarkEditor::Show() will create us then run away.  Unusually
  // for a controller, we are responsible for deallocating ourself.
  [self autorelease];
}

#pragma mark For Unit Test Use Only

- (NSString*)displayName {
  return [nameField_ stringValue];
}

- (NSString*)displayURL {
  return [urlField_ stringValue];
}

- (void)setDisplayName:(NSString*)name {
  [nameField_ setStringValue:name];
  [self controlTextDidChange:nil];
}

- (void)setDisplayURL:(NSString*)name {
  [urlField_ setStringValue:name];
  [self controlTextDidChange:nil];
}

- (BOOL)okButtonEnabled {
  return [okButton_ isEnabled];
}

- (void)selectTestNodeInBrowser:(const BookmarkNode*)node {
  [self selectNodeInBrowser:node];
}

#pragma mark NSBrowser delegate methods

// Given a column number, determine the parent bookmark folder node for the
// bookmarks being shown in that column.  This is done by scanning forward
// from column zero and following the selected row for each column up
// to the parent of the desired column.
- (const BookmarkNode*)parentNodeForColumn:(NSInteger)column {
  DCHECK(column >= 0);
  BookmarkModel* model = profile_->GetBookmarkModel();
  const BookmarkNode* node = model->root_node();
  for (NSInteger i = 0; i < column; ++i) {
    NSInteger selectedRowInColumn = [browser_ selectedRowInColumn:i];
    node = node->GetChild(selectedRowInColumn);
  }
  return node;
}

// This implementation returns the number of folders contained in the parent
// folder node for this column.
// TOTO(mrossetti): Decide if bookmark (i.e. non-folder) nodes should be
// shown, perhaps in gray.
- (NSInteger)browser:(NSBrowser*)sender numberOfRowsInColumn:(NSInteger)col {
  NSInteger rowCount = 0;
  const BookmarkNode* parentNode = [self parentNodeForColumn:col];
  if (parentNode) {
    int childCount = parentNode->GetChildCount();
    for (int i = 0; i < childCount; ++i) {
      const BookmarkNode* childNode = parentNode->GetChild(i);
      if (childNode->type() != BookmarkNode::URL)
        ++rowCount;
    }
  }
  return rowCount;
}

- (void)browser:(NSBrowser*)sender
    willDisplayCell:(NSBrowserCell*)cell
              atRow:(NSInteger)row
            column:(NSInteger)column {
  DCHECK(row >= 0);  // Trust AppKit, but verify.
  DCHECK(column >= 0);
  const BookmarkNode* parentNode = [self parentNodeForColumn:column];
  const BookmarkNode* childNode = GetFolderChildForParent(parentNode, row);
  DCHECK(childNode);
  [cell setTitle:base::SysWideToNSString(childNode->GetTitle())];
  [cell setLeaf:childNode->GetChildCount() == 0];
}

@end  // BookmarkEditorController

