// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AEROPEEK_MANAGER_H_
#define CHROME_BROWSER_AEROPEEK_MANAGER_H_
#pragma once

#include <windows.h>

#include <list>

#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "ui/gfx/insets.h"

namespace gfx {
class Size;
}
class AeroPeekWindow;
class SkBitmap;
class TabContents;

// A class which defines interfaces called from AeroPeekWindow.
// This class is used for dispatching an event received by a thumbnail window
// and for retrieving information from Chrome.
// An AeroPeek window receives the following events:
// * A user clicks an AeroPeek thumbnail.
//   We need to select a tab associated with this window.
// * A user closes an AeroPeek thumbnail.
//   We need to close a tab associated with this window.
// * A user clicks a toolbar button in an AeroPeek window.
//   We may need to dispatch this button event to a tab associated with this
//   thumbnail window.
//   <http://msdn.microsoft.com/en-us/library/dd378460(VS.85).aspx#thumbbars>.
// Also, it needs the following information of the browser:
// * The bitmap of a tab associated with this window.
//   This bitmap is used for creating thumbnail and preview images.
// * The rectangle of a browser frame.
//   This rectangle is used for pasting the above bitmap to the right position
//   and for marking the tab-content area as opaque.
// We assume these functions are called only from a UI thread (i.e.
// Chrome_BrowserMain).
class AeroPeekWindowDelegate {
 public:
  virtual void ActivateTab(int tab_id) = 0;
  virtual void CloseTab(int tab_id) = 0;
  virtual void GetContentInsets(gfx::Insets* insets) = 0;
  virtual bool GetTabThumbnail(int tab_id, SkBitmap* thumbnail) = 0;
  virtual bool GetTabPreview(int tab_id, SkBitmap* preview) = 0;

 protected:
  virtual ~AeroPeekWindowDelegate() {}
};

// A class that implements AeroPeek of Windows 7:
// <http://msdn.microsoft.com/en-us/library/dd378460(VS.85).aspx#thumbnails>.
// Windows 7 can dispay a thumbnail image of each tab to its taskbar so that
// a user can preview the contents of a tab (AeroPeek), choose a tab, close
// a tab, etc.
// This class implements the TabStripModelObserver interface to receive the
// following events sent from TabStripModel and dispatch them to Windows:
// * A tab is added.
//   This class adds a thumbnail window for this tab to the thumbnail list
//   of Windows.
// * A tab is being closed.
//   This class deletes the thumbnail window associated with this tab from the
//   thumbnail list of Windows.
// * A tab has been updated.
//   This class updates the image of the thumbnail window associated with this
//   tab.
// Also, this class receives events sent from Windows via thumbnail windows to
// TabStripModel:
// * A thumbnail window is closed.
//   Ask TabStrip to close the tab associated with this thumbnail window.
// * A thumbnail window is selected.
//   Ask TabStrip to activate the tab associated with this thumbnail window.
//
// The simplest usage of this class is:
// 1. Create an instance of TabThumbnailManager.
// 2. Add this instance to the observer list of a TabStrip object.
//
//      scoped_ptr<TabThumbnailManager> manager;
//      manager.reset(new TabThumbnailManager(
//          frame_->GetWindow()->GetNativeWindow(),
//          border_left,
//          border_top,
//          toolbar_top));
//      g_browser->tabstrip_model()->AddObserver(manager);
//
// 3. Remove this instance from the observer list of the TabStrip object when
//    we don't need it.
//
//      g_browser->tabstrip_model()->RemoveObserver(manager);
//
class AeroPeekManager : public TabStripModelObserver,
                        public AeroPeekWindowDelegate {
 public:
  explicit AeroPeekManager(HWND application_window);
  virtual ~AeroPeekManager();

  // Sets the margins of the "user-perceived content area".
  // (See comments of |content_insets_|).
  void SetContentInsets(const gfx::Insets& insets);

  // Returns whether or not we should enable Tab Thumbnailing and Aero Peek
  // of Windows 7.
  static bool Enabled();

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index);
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContentsWrapper* contents,
                        int from_index,
                        int to_index,
                        bool pinned_state_changed);
  virtual void TabChangedAt(TabContentsWrapper* contents,
                            int index,
                            TabChangeType change_type);
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index);

  // Overriden from TabThumbnailWindowDelegate:
  virtual void CloseTab(int tab_id);
  virtual void ActivateTab(int tab_id);
  virtual void GetContentInsets(gfx::Insets* insets);
  virtual bool GetTabThumbnail(int tab_id, SkBitmap* thumbnail);
  virtual bool GetTabPreview(int tab_id, SkBitmap* preview);

 private:
  // Deletes the TabThumbnailWindow object associated with the specified
  // Tab ID.
  void DeleteAeroPeekWindow(int tab_id);

  // If there is an AeroPeekWindow associated with |tab| it is removed and
  // deleted.
  void DeleteAeroPeekWindowForTab(TabContentsWrapper* tab);

  // Retrieves the AeroPeekWindow object associated with the specified
  // Tab ID.
  AeroPeekWindow* GetAeroPeekWindow(int tab_id) const;

  // If an AeroPeekWindow hasn't been created for |tab| yet, one is created.
  // |foreground| is true if the tab is selected.
  void CreateAeroPeekWindowIfNecessary(TabContentsWrapper* tab,
                                       bool foreground);

  // Returns a rectangle that fits into the destination rectangle and keeps
  // the pixel-aspect ratio of the source one.
  // (This function currently uses the longer-fit algorithm as IE8 does.)
  void GetOutputBitmapSize(const gfx::Size& destination,
                           const gfx::Size& source,
                           gfx::Size* output) const;

  // Returns the TabContents object associated with the specified Tab ID only
  // if it is alive.
  // Since Windows cannot send AeroPeek events directly to Chrome windows, we
  // use a place-holder window to receive AeroPeek events. So, when Windows
  // sends an AeroPeek event, the corresponding tab (and TabContents) may have
  // been deleted by Chrome. To prevent us from accessing deleted TabContents,
  // we need to check if the tab is still alive.
  TabContents* GetTabContents(int tab_id) const;

  // Returns the tab ID from the specified TabContents.
  int GetTabID(TabContents* contents) const;

 private:
  // The parent window of the place-holder windows used by AeroPeek.
  // In the case of Chrome, this window becomes a browser frame.
  HWND application_window_;

  // The list of the place-holder windows used by AeroPeek.
  std::list<AeroPeekWindow*> tab_list_;

  // The left and top borders of the frame window.
  // When we create a preview bitmap, we use these values for preventing from
  // over-writing the area of the browser frame.
  int border_left_;
  int border_top_;

  // The top position of the toolbar.
  // This value is used for setting the alpha values of the frame area so a
  // preview image can use transparent colors only in the frame area.
  int toolbar_top_;

  // The margins of the "user-perceived content area".
  // This value is used for pasting a tab image onto this "user-perceived
  // content area" when creating a preview image.
  gfx::Insets content_insets_;
};

#endif  // CHROME_BROWSER_AEROPEEK_MANAGER_H_
