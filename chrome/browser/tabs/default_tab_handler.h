// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_DEFAULT_TAB_HANDLER_H_
#define CHROME_BROWSER_TABS_DEFAULT_TAB_HANDLER_H_
#pragma once

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tabs/tab_handler.h"
#include "chrome/browser/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"

// A TabHandler implementation that interacts with the default TabStripModel.
// The intent is that the TabStripModel API is contained at this level, and
// never propagates beyond to the Browser.
class DefaultTabHandler : public TabHandler,
                          public TabStripModelDelegate,
                          public TabStripModelObserver {
 public:
  explicit DefaultTabHandler(TabHandlerDelegate* delegate);
  virtual ~DefaultTabHandler();

  // Overridden from TabHandler:
  virtual TabStripModel* GetTabStripModel() const OVERRIDE;

  // Overridden from TabStripModelDelegate:
  virtual TabContentsWrapper* AddBlankTab(bool foreground) OVERRIDE;
  virtual TabContentsWrapper* AddBlankTabAt(int index,
                                            bool foreground) OVERRIDE;
  virtual Browser* CreateNewStripWithContents(
      TabContentsWrapper* detached_contents,
      const gfx::Rect& window_bounds,
      const DockInfo& dock_info,
      bool maximize) OVERRIDE;
  virtual int GetDragActions() const OVERRIDE;
  virtual TabContentsWrapper* CreateTabContentsForURL(
      const GURL& url,
      const content::Referrer& referrer,
      Profile* profile,
      content::PageTransition transition,
      bool defer_load,
      SiteInstance* instance) const OVERRIDE;
  virtual bool CanDuplicateContentsAt(int index) OVERRIDE;
  virtual void DuplicateContentsAt(int index) OVERRIDE;
  virtual void CloseFrameAfterDragSession() OVERRIDE;
  virtual void CreateHistoricalTab(TabContentsWrapper* contents) OVERRIDE;
  virtual bool RunUnloadListenerBeforeClosing(
      TabContentsWrapper* contents) OVERRIDE;
  virtual bool CanCloseContents(std::vector<int>* indices) OVERRIDE;
  virtual bool CanBookmarkAllTabs() const OVERRIDE;
  virtual void BookmarkAllTabs() OVERRIDE;
  virtual bool CanCloseTab() const OVERRIDE;
  virtual bool CanRestoreTab() OVERRIDE;
  virtual void RestoreTab() OVERRIDE;
  virtual bool LargeIconsPermitted() const OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground) OVERRIDE;
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index) OVERRIDE;
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index) OVERRIDE;
  virtual void TabDeactivated(TabContentsWrapper* contents) OVERRIDE;
  virtual void ActiveTabChanged(TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabMoved(TabContentsWrapper* contents,
                        int from_index,
                        int to_index) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index) OVERRIDE;
  virtual void TabPinnedStateChanged(TabContentsWrapper* contents,
                                     int index) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;

 private:
  TabHandlerDelegate* delegate_;

  scoped_ptr<TabStripModel> model_;

  DISALLOW_COPY_AND_ASSIGN(DefaultTabHandler);
};

#endif  // CHROME_BROWSER_TABS_DEFAULT_TAB_HANDLER_H_
