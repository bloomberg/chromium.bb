// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_all_tabs_controller.h"
#include "app/l10n_util_mac.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

@implementation BookmarkAllTabsController

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
             configuration:(BookmarkEditor::Configuration)configuration
                   handler:(BookmarkEditor::Handler*)handler {
  NSString* nibName = @"BookmarkAllTabs";
  if ((self = [super initWithParentWindow:parentWindow
                                  nibName:nibName
                                  profile:profile
                                   parent:parent
                            configuration:configuration
                                  handler:handler])) {
  }
  return self;
}

- (void)awakeFromNib {
  [self setInitialName:
      l10n_util::GetNSStringWithFixup(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME)];
  [super awakeFromNib];
}

#pragma mark Bookmark Editing

- (void)UpdateActiveTabPairs {
  activeTabPairsVector_.clear();
  Browser* browser = BrowserList::GetLastActive();
  TabStripModel* tabstrip_model = browser->tabstrip_model();
  const int tabCount = tabstrip_model->count();
  for (int i = 0; i < tabCount; ++i) {
    TabContents* tc = tabstrip_model->GetTabContentsAt(i);
    const std::wstring tabTitle = UTF16ToWideHack(tc->GetTitle());
    const GURL& tabURL(tc->GetURL());
    ActiveTabNameURLPair tabPair(tabTitle, tabURL);
    activeTabPairsVector_.push_back(tabPair);
  }
}

// Called by -[BookmarkEditorBaseController ok:].  Creates the container
// folder for the tabs and then the bookmarks in that new folder.
// Returns a BOOL as an NSNumber indicating that the commit may proceed.
- (NSNumber*)didCommit {
  NSString* name = [[self displayName] stringByTrimmingCharactersInSet:
                    [NSCharacterSet newlineCharacterSet]];
  std::wstring newTitle = base::SysNSStringToWide(name);
  const BookmarkNode* newParentNode = [self selectedNode];
  int newIndex = newParentNode->GetChildCount();
  // Create the new folder which will contain all of the tab URLs.
  NSString* newFolderName = [self displayName];
  std::wstring newFolderString = base::SysNSStringToWide(newFolderName);
  BookmarkModel* model = [self bookmarkModel];
  const BookmarkNode* newFolder = model->AddGroup(newParentNode, newIndex,
                                                  newFolderString);
  [self notifyHandlerCreatedNode:newFolder];
  // Get a list of all open tabs, create nodes for them, and add
  // to the new folder node.
  [self UpdateActiveTabPairs];
  int i = 0;
  for (ActiveTabsNameURLPairVector::const_iterator it =
           activeTabPairsVector_.begin();
       it != activeTabPairsVector_.end(); ++it, ++i) {
    const BookmarkNode* node = model->AddURL(newFolder, i,
                                             it->first, it->second);
    [self notifyHandlerCreatedNode:node];
  }
  return [NSNumber numberWithBool:YES];
}

- (ActiveTabsNameURLPairVector*)activeTabPairsVector {
  return &activeTabPairsVector_;
}

@end  // BookmarkAllTabsController

