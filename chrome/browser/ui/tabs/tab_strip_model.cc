// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"

TabStripModel::TabStripModel(TabStripModelDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

TabStripModel::~TabStripModel() = default;

void TabStripModel::AddObserver(TabStripModelObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripModel::RemoveObserver(TabStripModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

// static
bool TabStripModel::ContextMenuCommandToBrowserCommand(int cmd_id,
                                                       int* browser_cmd) {
  switch (cmd_id) {
    case CommandNewTab:
      *browser_cmd = IDC_NEW_TAB;
      break;
    case CommandReload:
      *browser_cmd = IDC_RELOAD;
      break;
    case CommandDuplicate:
      *browser_cmd = IDC_DUPLICATE_TAB;
      break;
    case CommandCloseTab:
      *browser_cmd = IDC_CLOSE_TAB;
      break;
    case CommandRestoreTab:
      *browser_cmd = IDC_RESTORE_TAB;
      break;
    case CommandBookmarkAllTabs:
      *browser_cmd = IDC_BOOKMARK_ALL_TABS;
      break;
    default:
      *browser_cmd = 0;
      return false;
  }

  return true;
}

bool TabStripModel::ContainsWebContents(content::WebContents* contents) {
  return GetIndexOfWebContents(contents) != kNoTab;
}

void TabStripModel::OnWillDeleteWebContents(content::WebContents* contents,
                                            uint32_t close_types) {
  const int index = GetIndexOfWebContents(contents);
  DCHECK_NE(kNoTab, index);

  for (auto& observer : observers_)
    observer.TabClosingAt(this, contents, index);

  // Ask the delegate to save an entry for this tab in the historical tab
  // database if applicable.
  if ((close_types & CLOSE_CREATE_HISTORICAL_TAB) != 0)
    delegate_->CreateHistoricalTab(contents);
}

bool TabStripModel::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return delegate_->RunUnloadListenerBeforeClosing(contents);
}

bool TabStripModel::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return delegate_->ShouldRunUnloadListenerBeforeClosing(contents);
}
