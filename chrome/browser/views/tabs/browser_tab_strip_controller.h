// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_

#include "base/scoped_ptr.h"

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/tabs/side_tab_strip_model.h"

class SideTabStrip;

// An implementation of SideTabStripModel that sources data from
// the TabContentses in a TabStripModel.
class BrowserTabStripController : public SideTabStripModel,
                                  public TabStripModelObserver {
 public:
  BrowserTabStripController(TabStripModel* model, SideTabStrip* tabstrip);
  virtual ~BrowserTabStripController();

  void InitFromModel();

  bool IsCommandEnabledForTab(TabStripModel::ContextMenuCommand command_id,
                              int tab_index) const;
  bool IsCommandCheckedForTab(TabStripModel::ContextMenuCommand command_id,
                              int tab_index) const;
  void ExecuteCommandForTab(TabStripModel::ContextMenuCommand command_id,
                            int tab_index);

  // SideTabStripModel implementation:
  virtual SkBitmap GetIcon(int index) const;
  virtual string16 GetTitle(int index) const;
  virtual bool IsSelected(int index) const;
  virtual NetworkState GetNetworkState(int index) const;
  virtual void SelectTab(int index);
  virtual void CloseTab(int index);
  virtual void ShowContextMenu(int index, const gfx::Point& p);

  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(TabContents* contents, int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* contents, int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents, int from_index,
                        int to_index);
  virtual void TabChangedAt(TabContents* contents, int index,
                            TabChangeType change_type);
  virtual void TabReplacedAt(TabContents* old_contents,
                             TabContents* new_contents, int index);
  virtual void TabPinnedStateChanged(TabContents* contents, int index);
  virtual void TabBlockedStateChanged(TabContents* contents, int index);

 private:
  class TabContextMenuContents;

  TabStripModel* model_;
  SideTabStrip* tabstrip_;

  // If non-NULL it means we're showing a menu for the tab.
  scoped_ptr<TabContextMenuContents> context_menu_contents_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabStripController);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_

