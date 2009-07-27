// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"  // IDC_HISTORY_MENU
#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/history_menu_cocoa_controller.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "webkit/glue/window_open_disposition.h"  // CURRENT_TAB

@implementation HistoryMenuCocoaController

- (id)initWithBridge:(HistoryMenuBridge*)bridge {
  if ((self = [super init])) {
    bridge_ = bridge;
    DCHECK(bridge_);
  }
  return self;
}

// Open the URL of the given history item in the current tab.
- (void)openURLForItem:(HistoryMenuBridge::HistoryItem&)node {
  Browser* browser = BrowserList::GetLastActive();

  if (!browser) {  // No windows open?
    Browser::OpenEmptyWindow(bridge_->profile());
    browser = BrowserList::GetLastActive();
  }
  DCHECK(browser);
  TabContents* tab_contents = browser->GetSelectedTabContents();
  DCHECK(tab_contents);

  // A TabContents is a PageNavigator, so we can OpenURL() on it.
  tab_contents->OpenURL(node.url, GURL(), CURRENT_TAB,
                        PageTransition::AUTO_BOOKMARK);
}

- (HistoryMenuBridge::HistoryItem)itemForTag:(NSInteger)tag {
  std::vector<HistoryMenuBridge::HistoryItem>* results = NULL;
  NSInteger tag_base = 0;
  if (tag > IDC_HISTORY_MENU_VISITED && tag < IDC_HISTORY_MENU_CLOSED) {
    results = bridge_->visited_results();
    tag_base = IDC_HISTORY_MENU_VISITED;
  } else if (tag > IDC_HISTORY_MENU_CLOSED) {
    results = bridge_->closed_results();
    tag_base = IDC_HISTORY_MENU_CLOSED;
  } else {
    NOTREACHED();
  }

  DCHECK(tag > tag_base);
  size_t index = tag - tag_base - 1;
  if (index >= results->size())
    return HistoryMenuBridge::HistoryItem();
  return (*results)[index];
}

- (IBAction)openHistoryMenuItem:(id)sender {
  NSInteger tag = [sender tag];
  HistoryMenuBridge::HistoryItem item = [self itemForTag:tag];
  DCHECK(item.url.is_valid());
  [self openURLForItem:item];
}

@end  // HistoryMenuCocoaController
