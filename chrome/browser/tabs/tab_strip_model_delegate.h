// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_TABS_TAB_STRIP_MODEL_DELEGATE_H_
#pragma once

#include "content/common/page_transition_types.h"

class Browser;
class DockInfo;
class GURL;
class Profile;
class SiteInstance;
class TabContents;
class TabContentsWrapper;
namespace gfx {
class Rect;
}

///////////////////////////////////////////////////////////////////////////////
//
// TabStripModelDelegate
//
//  A delegate interface that the TabStripModel uses to perform work that it
//  can't do itself, such as obtain a container HWND for creating new
//  TabContents, creating new TabStripModels for detached tabs, etc.
//
//  This interface is typically implemented by the controller that instantiates
//  the TabStripModel (in our case the Browser object).
//
///////////////////////////////////////////////////////////////////////////////
class TabStripModelDelegate {
 public:
  enum {
    TAB_MOVE_ACTION = 1,
    TAB_TEAROFF_ACTION = 2
  };

  // Adds what the delegate considers to be a blank tab to the model.
  virtual TabContentsWrapper* AddBlankTab(bool foreground) = 0;
  virtual TabContentsWrapper* AddBlankTabAt(int index, bool foreground) = 0;

  // Asks for a new TabStripModel to be created and the given tab contents to
  // be added to it. Its size and position are reflected in |window_bounds|.
  // If |dock_info|'s type is other than NONE, the newly created window should
  // be docked as identified by |dock_info|. Returns the Browser object
  // representing the newly created window and tab strip. This does not
  // show the window, it's up to the caller to do so.
  virtual Browser* CreateNewStripWithContents(TabContentsWrapper* contents,
                                              const gfx::Rect& window_bounds,
                                              const DockInfo& dock_info,
                                              bool maximize) = 0;

  // Determines what drag actions are possible for the specified strip.
  virtual int GetDragActions() const = 0;

  // Creates an appropriate TabContents for the given URL. This is handled by
  // the delegate since the TabContents may require special circumstances to
  // exist for it to be constructed (e.g. a parent HWND).
  // If |defer_load| is true, the navigation controller doesn't load the url.
  // If |instance| is not null, its process is used to render the tab.
  virtual TabContentsWrapper* CreateTabContentsForURL(
      const GURL& url,
      const GURL& referrer,
      Profile* profile,
      PageTransition::Type transition,
      bool defer_load,
      SiteInstance* instance) const = 0;

  // Returns whether some contents can be duplicated.
  virtual bool CanDuplicateContentsAt(int index) = 0;

  // Duplicates the contents at the provided index and places it into its own
  // window.
  virtual void DuplicateContentsAt(int index) = 0;

  // Called when a drag session has completed and the frame that initiated the
  // the session should be closed.
  virtual void CloseFrameAfterDragSession() = 0;

  // Creates an entry in the historical tab database for the specified
  // TabContents.
  virtual void CreateHistoricalTab(TabContentsWrapper* contents) = 0;

  // Runs any unload listeners associated with the specified TabContents before
  // it is closed. If there are unload listeners that need to be run, this
  // function returns true and the TabStripModel will wait before closing the
  // TabContents. If it returns false, there are no unload listeners and the
  // TabStripModel can close the TabContents immediately.
  virtual bool RunUnloadListenerBeforeClosing(TabContentsWrapper* contents) = 0;

  // Returns true if a tab can be restored.
  virtual bool CanRestoreTab() = 0;

  // Restores the last closed tab if CanRestoreTab would return true.
  virtual void RestoreTab() = 0;

  // Returns whether some contents can be closed.
  virtual bool CanCloseContentsAt(int index) = 0;

  // Returns true if we should allow "bookmark all tabs" in this window; this is
  // true when there is more than one bookmarkable tab open.
  virtual bool CanBookmarkAllTabs() const = 0;

  // Creates a bookmark folder containing a bookmark for all open tabs.
  virtual void BookmarkAllTabs() = 0;

  // Returns true if any of the tabs can be closed.
  virtual bool CanCloseTab() const = 0;

  // Returns true if the vertical tabstrip presentation should be used.
  virtual bool UseVerticalTabs() const = 0;

  // Toggles the use of the vertical tabstrip.
  virtual void ToggleUseVerticalTabs() = 0;

  // Returns true if the tab strip can use large icons.
  virtual bool LargeIconsPermitted() const = 0;

 protected:
  virtual ~TabStripModelDelegate() {}
};

#endif  // CHROME_BROWSER_TABS_TAB_STRIP_MODEL_DELEGATE_H_
