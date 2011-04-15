// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the public interface for the blocked content (including popup)
// notifications. This interface should only be used by TabContents. Users and
// subclasses of TabContents should use the appropriate methods on TabContents
// to access information about blocked content.

#ifndef CHROME_BROWSER_BLOCKED_CONTENT_CONTAINER_H_
#define CHROME_BROWSER_BLOCKED_CONTENT_CONTAINER_H_
#pragma once

#include <vector>

#include "content/browser/tab_contents/tab_contents_delegate.h"

// Takes ownership of TabContents that are unrequested popup windows.
class BlockedContentContainer : public TabContentsDelegate {
 public:
  // Creates a container for a certain TabContents:
  explicit BlockedContentContainer(TabContents* owner);
  virtual ~BlockedContentContainer();

  // Adds a TabContents to this container. |bounds| are the window bounds
  // requested for the TabContents.
  void AddTabContents(TabContents* tab_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& bounds,
                      bool user_gesture);

  // Shows the blocked TabContents |tab_contents|.
  void LaunchForContents(TabContents* tab_contents);

  // Returns the number of blocked contents.
  size_t GetBlockedContentsCount() const;

  // Returns the contained TabContents pointers.  |blocked_contents| must be
  // non-NULL.
  void GetBlockedContents(std::vector<TabContents*>* blocked_contents) const;

  // Sets this object up to delete itself.
  void Destroy();

  // Overridden from TabContentsDelegate:

  // Forwards OpenURLFromTab to our |owner_|.
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);

  // Ignored; BlockedContentContainer doesn't display a throbber.
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}

  // Forwards AddNewContents to our |owner_|.
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_position,
                              bool user_gesture);

  // Ignore activation/deactivation requests from the TabContents we're
  // blocking.
  virtual void ActivateContents(TabContents* contents) {}
  virtual void DeactivateContents(TabContents* contents) {}

  // Ignored; BlockedContentContainer doesn't display a throbber.
  virtual void LoadingStateChanged(TabContents* source) {}

  // Removes |source| from our internal list of blocked contents.
  virtual void CloseContents(TabContents* source);

  // Changes the opening rectangle associated with |source|.
  virtual void MoveContents(TabContents* source, const gfx::Rect& new_bounds);

  // Always returns true.
  virtual bool IsPopup(const TabContents* source) const;

  // Returns our |owner_|.
  virtual TabContents* GetConstrainingContents(TabContents* source);

  // Ignored; BlockedContentContainer doesn't display a URL bar.
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}

  // Maximum number of blocked contents we allow. No page should really need
  // this many anyway. If reached it typically means there is a compromised
  // renderer.
  static const size_t kImpossibleNumberOfPopups;

 private:
  struct BlockedContent;

  typedef std::vector<BlockedContent> BlockedContents;

  // The TabContents that owns and constrains this BlockedContentContainer.
  TabContents* owner_;

  // Information about all blocked contents.
  BlockedContents blocked_contents_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlockedContentContainer);
};

#endif  // CHROME_BROWSER_BLOCKED_CONTENT_CONTAINER_H_
