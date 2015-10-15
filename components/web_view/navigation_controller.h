// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_NAVIGATION_CONTROLLER_H_
#define COMPONENTS_WEB_VIEW_NAVIGATION_CONTROLLER_H_

#include "base/memory/scoped_vector.h"

#include "components/web_view/public/interfaces/web_view.mojom.h"
#include "url/gurl.h"

namespace web_view {

class Frame;
class NavigationEntry;
class NavigationControllerDelegate;
enum class ReloadType;

// A NavigationController maintains the back-forward list for a WebView and
// manages all navigation within that list.
//
// Each NavigationController belongs to one WebContents; each WebContents has
// exactly one NavigationController.
class NavigationController {
 public:
  explicit NavigationController(NavigationControllerDelegate* delegate);
  ~NavigationController();

  int GetCurrentEntryIndex() const;
  int GetIndexForOffset(int offset) const;
  int GetEntryCount() const;
  NavigationEntry* GetEntryAtIndex(int index) const;
  NavigationEntry* GetEntryAtOffset(int offset) const;
  bool CanGoBack() const;
  bool CanGoForward() const;
  bool CanGoToOffset(int offset) const;

  void GoBack();
  void GoForward();

  void LoadURL(mojo::URLRequestPtr request);

  void NavigateToPendingEntry(ReloadType reload_type,
                              bool update_navigation_start_time);

  // Takes ownership of a pending entry, and adds it to the current list.
  //
  // TODO(erg): This should eventually own the navigation transition like
  // content::NavigationControllerImpl::NavigateToPendingEntry() does.
  void SetPendingEntry(scoped_ptr<NavigationEntry> entry);

  // Discards only the pending entry. |was_failure| should be set if the pending
  // entry is being discarded because it failed to load.
  void DiscardPendingEntry(bool was_failure);

  // Called when a frame is committed.
  void FrameDidCommitProvisionalLoad(Frame* frame);

  // Called when a frame navigated by itself. Adds the new url to the
  // back/forward stack.
  void FrameDidNavigateLocally(Frame* frame, const GURL& url);

 private:
  using NavigationEntries = ScopedVector<NavigationEntry>;

  void ClearForwardEntries();

  NavigationEntries entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  //
  // This may refer to an item in the entries_ list if the pending_entry_index_
  // == -1, or it may be its own entry that should be deleted. Be careful with
  // the memory management.
  NavigationEntry* pending_entry_;

  // The index of the currently visible entry.
  int last_committed_entry_index_;

  // The index of the pending entry if it is in entries_, or -1 if
  // pending_entry_ is a new entry (created by LoadURL).
  int pending_entry_index_;

  NavigationControllerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NavigationController);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_NAVIGATION_CONTROLLER_H_
