// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_editor_base_controller.h"
#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_all_tabs_controller.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#import "chrome/browser/cocoa/bookmark_tree_browser_cell.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

@interface BookmarkEditorBaseController (Private)

// Given a cell in the folder browser, make that cell editable so that the
// bookmark folder name can be modified by the user.
- (void)editFolderNameInCell:(BookmarkTreeBrowserCell*)cell;

// The action called by the bookmark folder name cell being edited by
// the user when editing has been completed (such as by pressing <return>).
- (void)cellEditingCompleted:(id)sender;

// Update the folder name from the current edit in the given cell
// and return the focus to the folder tree browser.
- (void)saveFolderNameForCell:(BookmarkTreeBrowserCell*)cell;

// A custom action handler called by the bookmark folder browser when the
// user has double-clicked on a folder name.
- (void)browserDoubleClicked:(id)sender;

@end

// static; implemented for each platform.
void BookmarkEditor::Show(gfx::NativeWindow parent_hwnd,
                          Profile* profile,
                          const BookmarkNode* parent,
                          const EditDetails& details,
                          Configuration configuration,
                          Handler* handler) {
  BookmarkEditorBaseController* controller = nil;
  if (details.type == EditDetails::NEW_FOLDER) {
    controller = [[BookmarkAllTabsController alloc]
                  initWithParentWindow:parent_hwnd
                               profile:profile
                                parent:parent
                         configuration:configuration
                               handler:handler];
  } else {
    controller = [[BookmarkEditorController alloc]
                  initWithParentWindow:parent_hwnd
                               profile:profile
                                parent:parent
                                  node:details.existing_node
                         configuration:configuration
                               handler:handler];
  }
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

@implementation BookmarkEditorBaseController

@synthesize initialName = initialName_;
@synthesize displayName = displayName_;
@synthesize okEnabled = okEnabled_;

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   nibName:(NSString*)nibName
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
             configuration:(BookmarkEditor::Configuration)configuration
                   handler:(BookmarkEditor::Handler*)handler {
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:nibName
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    parentWindow_ = parentWindow;
    profile_ = profile;
    parentNode_ = parent;
    configuration_ = configuration;
    handler_.reset(handler);
    initialName_ = [@"" retain];
  }
  return self;
}

- (void)dealloc {
  [initialName_ release];
  [displayName_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  [self setDisplayName:[self initialName]];

  if (configuration_ != BookmarkEditor::SHOW_TREE) {
    // Remember the NSBrowser's height; we will shrink our frame by that
    // much.
    NSRect frame = [[self window] frame];
    CGFloat browserHeight = [folderBrowser_ frame].size.height;
    frame.size.height -= browserHeight;
    frame.origin.y += browserHeight;
    // Remove the NSBrowser and "new folder" button.
    [folderBrowser_ removeFromSuperview];
    [newFolderButton_ removeFromSuperview];
    // Finally, commit the size change.
    [[self window] setFrame:frame display:YES];
  }

  [folderBrowser_ setCellClass:[BookmarkTreeBrowserCell class]];
  [folderBrowser_ setDoubleAction:@selector(browserDoubleClicked:)];
}

- (void)windowDidLoad {
  if (configuration_ == BookmarkEditor::SHOW_TREE) {
    [self selectNodeInBrowser:parentNode_];
  }
}

- (void)windowWillClose:(NSNotification *)notification {
  // If a folder name cell is being edited then force it to end editing
  // so that any changes are recorded.
  [[self window] makeFirstResponder:nil];
  [self autorelease];
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

- (void)selectNodeInBrowser:(const BookmarkNode*)node {
  DCHECK(configuration_ == BookmarkEditor::SHOW_TREE);
  std::deque<NSInteger> rowsToSelect;
  const BookmarkNode* nodeParent = nil;
  if (node) {
    nodeParent = node->GetParent();
    // There should always be a parent node.
    DCHECK(nodeParent);
    while (nodeParent) {
      int nodeRow = IndexOfFolderChild(node);
      rowsToSelect.push_front(nodeRow);
      node = nodeParent;
      nodeParent = nodeParent->GetParent();
    }
  } else {
    BookmarkModel* model = profile_->GetBookmarkModel();
    nodeParent = model->GetBookmarkBarNode();
    rowsToSelect.push_front(0);
  }
  for (std::deque<NSInteger>::size_type column = 0;
       column < rowsToSelect.size();
       ++column) {
    [folderBrowser_ selectRow:rowsToSelect[column] inColumn:column];
  }

  // Force the OK button state to be re-evaluated.
  [self willChangeValueForKey:@"okEnabled"];
  [self didChangeValueForKey:@"okEnabled"];
}

- (const BookmarkNode*)selectedNode {
  BookmarkModel* model = profile_->GetBookmarkModel();
  const BookmarkNode* selectedNode = NULL;
  // Determine a new parent node only if the browser is showing.
  if (configuration_ == BookmarkEditor::SHOW_TREE) {
    selectedNode = model->root_node();
    NSInteger column = 0;
    NSInteger selectedRow = [folderBrowser_ selectedRowInColumn:column];
    while (selectedRow >= 0) {
      selectedNode = GetFolderChildForParent(selectedNode,
                                             selectedRow);
      ++column;
      selectedRow = [folderBrowser_ selectedRowInColumn:column];
    }
  } else {
    // If the tree is not showing then we use the original parent.
    selectedNode = parentNode_;
  }
  return selectedNode;
}

- (void)NotifyHandlerCreatedNode:(const BookmarkNode*)node {
  if (handler_.get())
    handler_->NodeCreated(node);
}

#pragma mark New Folder Handler & Folder Cell Editing

- (void)editFolderNameInCell:(BookmarkTreeBrowserCell*)cell {
  DCHECK([cell isKindOfClass:[BookmarkTreeBrowserCell class]]);
  [cell setEditable:YES];
  [cell setTarget:self];
  [cell setAction:@selector(cellEditingCompleted:)];
  [cell setSendsActionOnEndEditing:YES];
  NSMatrix* matrix = [cell matrix];
  // Set the delegate so that we get called when editing completes.
  [matrix setDelegate:self];
  [matrix selectText:self];
}

- (void)cellEditingCompleted:(id)sender {
  DCHECK([sender isKindOfClass:[NSMatrix class]]);
  BookmarkTreeBrowserCell* cell = [sender selectedCell];
  DCHECK([cell isKindOfClass:[BookmarkTreeBrowserCell class]]);
  [self saveFolderNameForCell:cell];
}

- (void)saveFolderNameForCell:(BookmarkTreeBrowserCell*)cell {
  DCHECK([cell isKindOfClass:[BookmarkTreeBrowserCell class]]);
  // It's possible that the cell can get reused so clean things up
  // to prevent inadvertant notifications.
  [cell setTarget:nil];
  [cell setAction:nil];
  [cell setEditable:NO];
  [cell setSendsActionOnEndEditing:NO];
  // Force a responder change here to force the editing of the cell's text
  // to complete otherwise the call to -[cell title] could return stale text.
  // The focus does not automatically get reset to the browser when the
  // cell gives up focus.
  [[folderBrowser_ window] makeFirstResponder:folderBrowser_];
  const BookmarkNode* bookmarkNode = [cell bookmarkNode];
  BookmarkModel* model = profile_->GetBookmarkModel();
  NSString* newTitle = [cell title];
  model->SetTitle(bookmarkNode, base::SysNSStringToWide(newTitle));
}

- (void)browserDoubleClicked:(id)sender {
  BookmarkTreeBrowserCell* cell = [folderBrowser_ selectedCell];
  DCHECK([cell isKindOfClass:[BookmarkTreeBrowserCell class]]);
  [self editFolderNameInCell:cell];
}

- (IBAction)newFolder:(id)sender {
  BookmarkModel* model = profile_->GetBookmarkModel();
  const BookmarkNode* newParentNode = [self selectedNode];
  int newIndex = newParentNode->GetChildCount();
  std::wstring newFolderString =
      l10n_util::GetString(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME);
  const BookmarkNode* newFolder = model->AddGroup(newParentNode, newIndex,
                                                  newFolderString);
  [self selectNodeInBrowser:newFolder];
  BookmarkTreeBrowserCell* cell = [folderBrowser_ selectedCell];
  [self editFolderNameInCell:cell];
}

- (BOOL)okEnabled {
  return YES;
}

- (IBAction)ok:(id)sender {
  [NSApp endSheet:[self window]];
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window]];
}

- (void)didEndSheet:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  [sheet close];
}

- (BookmarkModel*)bookmarkModel {
  return profile_->GetBookmarkModel();
}

- (const BookmarkNode*)parentNode {
  return parentNode_;
}

#pragma mark For Unit Test Use Only

- (BOOL)okButtonEnabled {
  return [okButton_ isEnabled];
}

- (void)selectTestNodeInBrowser:(const BookmarkNode*)node {
  [self selectNodeInBrowser:node];
}

+ (const BookmarkNode*)folderChildForParent:(const BookmarkNode*)parent
                            withFolderIndex:(NSInteger)index {
  return GetFolderChildForParent(parent, index);
}

+ (int)indexOfFolderChild:(const BookmarkNode*)child {
  return IndexOfFolderChild(child);
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
    NSInteger selectedRowInColumn = [folderBrowser_ selectedRowInColumn:i];
    node = GetFolderChildForParent(node, selectedRowInColumn);
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
  DCHECK([cell isKindOfClass:[BookmarkTreeBrowserCell class]]);
  const BookmarkNode* parentNode = [self parentNodeForColumn:column];
  const BookmarkNode* childNode = GetFolderChildForParent(parentNode, row);
  DCHECK(childNode);
  BookmarkTreeBrowserCell* browserCell =
      static_cast<BookmarkTreeBrowserCell*>(cell);
  [browserCell setTitle:base::SysWideToNSString(childNode->GetTitle())];
  [browserCell setBookmarkNode:childNode];
  [browserCell setMatrix:[folderBrowser_ matrixInColumn:column]];
}

@end  // BookmarkEditorBaseController

