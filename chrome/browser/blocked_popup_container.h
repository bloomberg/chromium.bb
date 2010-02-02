// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the public interface for the blocked popup notifications. This
// interface should only be used by TabContents. Users and subclasses of
// TabContents should use the appropriate methods on TabContents to access
// information about blocked popups.

#ifndef CHROME_BROWSER_BLOCKED_POPUP_CONTAINER_H_
#define CHROME_BROWSER_BLOCKED_POPUP_CONTAINER_H_

#include "chrome/browser/tab_contents/tab_contents_delegate.h"

// Takes ownership of TabContents that are unrequested popup windows.
class BlockedPopupContainer : public TabContentsDelegate {
 public:
  typedef std::vector<TabContents*> BlockedContents;

  // Creates a container for a certain TabContents:
  explicit BlockedPopupContainer(TabContents* owner);

  // Adds a popup to this container. |bounds| are the window bounds requested by
  // the popup window.
  void AddTabContents(TabContents* tab_contents,
                      const gfx::Rect& bounds);

  // Shows the blocked popup with TabContents |tab_contents|.
  void LaunchPopupForContents(TabContents* tab_contents);

  // Returns the number of blocked popups.
  size_t GetBlockedPopupCount() const;

  // Returns the contained TabContents pointers.  |blocked_contents| must be
  // non-NULL.
  void GetBlockedContents(BlockedContents* blocked_contents) const;

  // Sets this object up to delete itself.
  void Destroy();

  // Overridden from TabContentsDelegate:

  // Forwards OpenURLFromTab to our |owner_|.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);

  // Ignored; BlockedPopupContainer doesn't display a throbber.
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}

  // Forwards AddNewContents to our |owner_|.
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_position,
                              bool user_gesture);

  // Ignore activation requests from the TabContents we're blocking.
  virtual void ActivateContents(TabContents* contents) {}

  // Ignored; BlockedPopupContainer doesn't display a throbber.
  virtual void LoadingStateChanged(TabContents* source) {}

  // Removes |source| from our internal list of blocked popups.
  virtual void CloseContents(TabContents* source);

  // Changes the opening rectangle associated with |source|.
  virtual void MoveContents(TabContents* source, const gfx::Rect& new_bounds);

  // Always returns true.
  virtual bool IsPopup(TabContents* source);

  // Returns our |owner_|.
  virtual TabContents* GetConstrainingContents(TabContents* source);

  // Ignored; BlockedPopupContainer doesn't display a toolbar.
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}

  // Ignored; BlockedPopupContainer doesn't display a bookmarking star.
  virtual void URLStarredChanged(TabContents* source, bool starred) {}

  // Ignored; BlockedPopupContainer doesn't display a URL bar.
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}

  // A number larger than the internal popup count on the Renderer; meant for
  // preventing a compromised renderer from exhausting GDI memory by spawning
  // infinite windows.
  static const size_t kImpossibleNumberOfPopups;

 protected:
  struct BlockedPopup {
    BlockedPopup(TabContents* tab_contents,
                 const gfx::Rect& bounds)
        : tab_contents(tab_contents), bounds(bounds) {
    }

    TabContents* tab_contents;
    gfx::Rect bounds;
  };
  typedef std::vector<BlockedPopup> BlockedPopups;

 private:
  // The TabContents that owns and constrains this BlockedPopupContainer.
  TabContents* owner_;

  // Information about all blocked popups.
  BlockedPopups blocked_popups_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlockedPopupContainer);
};

#endif  // CHROME_BROWSER_BLOCKED_POPUP_CONTAINER_H_
