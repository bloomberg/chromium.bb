// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/page_transition_types.h"

class GURL;

namespace content {

class BrowserContext;
class NavigationEntry;
class SessionStorageNamespace;
class WebContents;
struct Referrer;

// A NavigationController maintains the back-forward list for a WebContents and
// manages all navigation within that list.
//
// Each NavigationController belongs to one WebContents; each WebContents has
// exactly one NavigationController.
class NavigationController {
 public:
  enum ReloadType {
    NO_RELOAD,                // Normal load.
    RELOAD,                   // Normal (cache-validating) reload.
    RELOAD_IGNORING_CACHE     // Reload bypassing the cache, aka shift-reload.
  };

  // Creates a navigation entry and translates the virtual url to a real one.
  // This is a general call; prefer LoadURL[FromRenderer]/TransferURL below.
  // Extra headers are separated by \n.
  CONTENT_EXPORT static NavigationEntry* CreateNavigationEntry(
      const GURL& url,
      const Referrer& referrer,
      PageTransition transition,
      bool is_renderer_initiated,
      const std::string& extra_headers,
      BrowserContext* browser_context);

  // Disables checking for a repost and prompting the user. This is used during
  // testing.
  CONTENT_EXPORT static void DisablePromptOnRepost();

  virtual ~NavigationController() {}

  // Returns the web contents associated with this controller. It can never be
  // NULL.
  virtual WebContents* GetWebContents() const = 0;

  // Get/set the browser context for this controller. It can never be NULL.
  virtual BrowserContext* GetBrowserContext() const = 0;
  virtual void SetBrowserContext(BrowserContext* browser_context) = 0;

  // Initializes this NavigationController with the given saved navigations,
  // using selected_navigation as the currently loaded entry. Before this call
  // the controller should be unused (there should be no current entry). If
  // from_last_session is true, navigations are from the previous session,
  // otherwise they are from the current session (undo tab close). This takes
  // ownership of the NavigationEntrys in |entries| and clears it out.
  // This is used for session restore.
  virtual void Restore(int selected_navigation,
                       bool from_last_session,
                       std::vector<NavigationEntry*>* entries) = 0;

  // Entries -------------------------------------------------------------------

  // There are two basic states for entries: pending and committed. When an
  // entry is navigated to, a request is sent to the server. While that request
  // has not been responded to, the NavigationEntry is pending. Once data is
  // received for that entry, that NavigationEntry is committed.

  // A transient entry is an entry that, when the user navigates away, is
  // removed and discarded rather than being added to the back-forward list.
  // Transient entries are useful for interstitial pages and the like.

  // Active entry --------------------------------------------------------------

  // Returns the active entry, which is the transient entry if any, the pending
  // entry if a navigation is in progress or the last committed entry otherwise.
  // NOTE: This can be NULL!!
  //
  // If you are trying to get the current state of the NavigationController,
  // this is the method you will typically want to call.  If you want to display
  // the active entry to the user (e.g., in the location bar), use
  // GetVisibleEntry instead.
  virtual NavigationEntry* GetActiveEntry() const = 0;

  // Returns the same entry as GetActiveEntry, except that it ignores pending
  // history navigation entries.  This should be used when displaying info to
  // the user, so that the location bar and other indicators do not update for
  // a back/forward navigation until the pending entry commits.  This approach
  // guards against URL spoofs on slow history navigations.
  virtual NavigationEntry* GetVisibleEntry() const = 0;

  // Returns the index from which we would go back/forward or reload.  This is
  // the last_committed_entry_index_ if pending_entry_index_ is -1.  Otherwise,
  // it is the pending_entry_index_.
  virtual int GetCurrentEntryIndex() const = 0;

  // Returns the last committed entry, which may be null if there are no
  // committed entries.
  virtual NavigationEntry* GetLastCommittedEntry() const = 0;

  // Returns the index of the last committed entry.
  virtual int GetLastCommittedEntryIndex() const = 0;

  // Returns true if the source for the current entry can be viewed.
  virtual bool CanViewSource() const = 0;

  // Navigation list -----------------------------------------------------------

  // Returns the number of entries in the NavigationController, excluding
  // the pending entry if there is one, but including the transient entry if
  // any.
  virtual int GetEntryCount() const = 0;

  virtual NavigationEntry* GetEntryAtIndex(int index) const = 0;

  // Returns the entry at the specified offset from current.  Returns NULL
  // if out of bounds.
  virtual NavigationEntry* GetEntryAtOffset(int offset) const = 0;

  // Pending entry -------------------------------------------------------------

  // Discards the pending and transient entries if any.
  virtual void DiscardNonCommittedEntries() = 0;

  // Returns the pending entry corresponding to the navigation that is
  // currently in progress, or null if there is none.
  virtual NavigationEntry* GetPendingEntry() const = 0;

  // Returns the index of the pending entry or -1 if the pending entry
  // corresponds to a new navigation (created via LoadURL).
  virtual int GetPendingEntryIndex() const = 0;

  // Transient entry -----------------------------------------------------------

  // Returns the transient entry if any. This is an entry which is removed and
  // discarded if any navigation occurs. Note that the returned entry is owned
  // by the navigation controller and may be deleted at any time.
  virtual NavigationEntry* GetTransientEntry() const = 0;

  // New navigations -----------------------------------------------------------

  // Loads the specified URL, specifying extra http headers to add to the
  // request.  Extra headers are separated by \n.
  virtual void LoadURL(const GURL& url,
                       const Referrer& referrer,
                       PageTransition type,
                       const std::string& extra_headers) = 0;

  // Same as LoadURL, but for renderer-initiated navigations.  This state is
  // important for tracking whether to display pending URLs.
  virtual void LoadURLFromRenderer(const GURL& url,
                                   const Referrer& referrer,
                                   PageTransition type,
                                   const std::string& extra_headers) = 0;

  // Same as LoadURL, but allows overriding the user agent of the
  // NavigationEntry before it loads.
  // TODO(dfalcantara): Consolidate the LoadURL* interfaces.
  virtual void LoadURLWithUserAgentOverride(const GURL& url,
                                            const Referrer& referrer,
                                            PageTransition type,
                                            bool is_renderer_initiated,
                                            const std::string& extra_headers,
                                            bool is_overriding_user_agent) = 0;

  // Behaves like LoadURL() and LoadURLFromRenderer() but marks the new
  // navigation as being transferred from one RVH to another. In this case the
  // browser can recycle the old request once the new renderer wants to
  // navigate.
  // |transferred_global_request_id| identifies the request ID of the old
  // request.
  virtual void TransferURL(
      const GURL& url,
      const Referrer& referrer,
      PageTransition transition,
      const std::string& extra_headers,
      const GlobalRequestID& transferred_global_request_id,
      bool is_renderer_initiated) = 0;

  // Loads the current page if this NavigationController was restored from
  // history and the current page has not loaded yet.
  virtual void LoadIfNecessary() = 0;

  // Renavigation --------------------------------------------------------------

  // Navigation relative to the "current entry"
  virtual bool CanGoBack() const = 0;
  virtual bool CanGoForward() const = 0;
  virtual void GoBack() = 0;
  virtual void GoForward() = 0;

  // Navigates to the specified absolute index.
  virtual void GoToIndex(int index) = 0;

  // Navigates to the specified offset from the "current entry". Does nothing if
  // the offset is out of bounds.
  virtual void GoToOffset(int offset) = 0;

  // Reloads the current entry. If |check_for_repost| is true and the current
  // entry has POST data the user is prompted to see if they really want to
  // reload the page. In nearly all cases pass in true.
  virtual void Reload(bool check_for_repost) = 0;

  // Like Reload(), but don't use caches (aka "shift-reload").
  virtual void ReloadIgnoringCache(bool check_for_repost) = 0;

  // Removing of entries -------------------------------------------------------

  // Removes the entry at the specified |index|.  This call dicards any pending
  // and transient entries.  If the index is the last committed index, this does
  // nothing and returns false.
  virtual void RemoveEntryAtIndex(int index) = 0;

  // Random --------------------------------------------------------------------

  // The session storage namespace that all child render views should use.
  virtual SessionStorageNamespace* GetSessionStorageNamespace() const = 0;

  // Sets the max restored page ID this NavigationController has seen, if it
  // was restored from a previous session.
  virtual void SetMaxRestoredPageID(int32 max_id) = 0;

  // Returns the largest restored page ID seen in this navigation controller,
  // if it was restored from a previous session.  (-1 otherwise)
  virtual int32 GetMaxRestoredPageID() const = 0;

  // Returns true if a reload happens when activated (SetActive(true) is
  // invoked). This is true for session/tab restore and cloned tabs.
  virtual bool NeedsReload() const = 0;

  // Cancels a repost that brought up a warning.
  virtual void CancelPendingReload() = 0;
  // Continues a repost that brought up a warning.
  virtual void ContinuePendingReload() = 0;

  // Returns true if we are navigating to the URL the tab is opened with.
  virtual bool IsInitialNavigation() = 0;

  // Broadcasts the NOTIFY_NAV_ENTRY_CHANGED notification for the given entry
  // (which must be at the given index). This will keep things in sync like
  // the saved session.
  virtual void NotifyEntryChanged(const NavigationEntry* entry, int index) = 0;

  // Copies the navigation state from the given controller to this one. This
  // one should be empty (just created).
  virtual void CopyStateFrom(const NavigationController& source) = 0;

  // A variant of CopyStateFrom. Removes all entries from this except the last
  // entry, inserts all entries from |source| before and including the active
  // entry. This method is intended for use when the last entry of |this| is the
  // active entry. For example:
  // source: A B *C* D
  // this:   E F *G*   (last must be active or pending)
  // result: A B C *G*
  // This ignores the transient index of the source and honors that of 'this'.
  virtual void CopyStateFromAndPrune(NavigationController* source) = 0;

  // Removes all the entries except the active entry. If there is a new pending
  // navigation it is preserved.
  virtual void PruneAllButActive() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_
