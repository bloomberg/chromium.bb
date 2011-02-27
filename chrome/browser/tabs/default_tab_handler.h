// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_DEFAULT_TAB_HANDLER_H_
#define CHROME_BROWSER_TABS_DEFAULT_TAB_HANDLER_H_
#pragma once

#include "base/scoped_ptr.h"
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
  virtual TabStripModel* GetTabStripModel() const;

  // Overridden from TabStripModelDelegate:
  virtual TabContentsWrapper* AddBlankTab(bool foreground);
  virtual TabContentsWrapper* AddBlankTabAt(int index, bool foreground);
  virtual Browser* CreateNewStripWithContents(
      TabContentsWrapper* detached_contents,
      const gfx::Rect& window_bounds,
      const DockInfo& dock_info,
      bool maximize);
  virtual int GetDragActions() const;
  virtual TabContentsWrapper* CreateTabContentsForURL(
      const GURL& url,
      const GURL& referrer,
      Profile* profile,
      PageTransition::Type transition,
      bool defer_load,
      SiteInstance* instance) const;
  virtual bool CanDuplicateContentsAt(int index);
  virtual void DuplicateContentsAt(int index);
  virtual void CloseFrameAfterDragSession();
  virtual void CreateHistoricalTab(TabContentsWrapper* contents);
  virtual bool RunUnloadListenerBeforeClosing(TabContentsWrapper* contents);
  virtual bool CanCloseContentsAt(int index);
  virtual bool CanBookmarkAllTabs() const;
  virtual void BookmarkAllTabs();
  virtual bool CanCloseTab() const;
  virtual void ToggleUseVerticalTabs();
  virtual bool CanRestoreTab();
  virtual void RestoreTab();
  virtual bool LargeIconsPermitted() const;
  virtual bool UseVerticalTabs() const;

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground);
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index);
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index);
  virtual void TabDeselected(TabContentsWrapper* contents);
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContentsWrapper* contents,
                        int from_index,
                        int to_index);
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index);
  virtual void TabPinnedStateChanged(TabContentsWrapper* contents, int index);
  virtual void TabStripEmpty();

 private:
  TabHandlerDelegate* delegate_;

  scoped_ptr<TabStripModel> model_;

  DISALLOW_COPY_AND_ASSIGN(DefaultTabHandler);
};

#endif  // CHROME_BROWSER_TABS_DEFAULT_TAB_HANDLER_H_

