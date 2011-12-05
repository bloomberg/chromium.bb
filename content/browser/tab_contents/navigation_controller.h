// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_CONTROLLER_H_
#define CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_CONTROLLER_H_
#pragma once

#include "build/build_config.h"

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"

class NavigationEntry;
class SessionStorageNamespace;
class SiteInstance;
class TabContents;
struct ViewHostMsg_FrameNavigate_Params;

namespace content {
class BrowserContext;
struct LoadCommittedDetails;
struct Referrer;
}

// A NavigationController maintains the back-forward list for a single tab and
// manages all navigation within that list.
//
// The NavigationController also owns all TabContents for the tab. This is to
// make sure that we have at most one TabContents instance per type.
class CONTENT_EXPORT NavigationController {
 public:

  enum ReloadType {
    NO_RELOAD,                // Normal load.
    RELOAD,                   // Normal (cache-validating) reload.
    RELOAD_IGNORING_CACHE     // Reload bypassing the cache, aka shift-reload.
  };

  // ---------------------------------------------------------------------------

  NavigationController(TabContents* tab_contents,
                       content::BrowserContext* browser_context,
                       SessionStorageNamespace* session_storage_namespace);
  ~NavigationController();

  // Returns the browser context for this controller. It can never be NULL.
  content::BrowserContext* browser_context() const {
    return browser_context_;
  }

  // Sets the browser context for this controller.
  void set_browser_context(content::BrowserContext* browser_context) {
    browser_context_ = browser_context;
  }

  // Initializes this NavigationController with the given saved navigations,
  // using selected_navigation as the currently loaded entry. Before this call
  // the controller should be unused (there should be no current entry). If
  // from_last_session is true, navigations are from the previous session,
  // otherwise they are from the current session (undo tab close). This takes
  // ownership of the NavigationEntrys in |entries| and clears it out.
  // This is used for session restore.
  void Restore(int selected_navigation,
               bool from_last_session,
               std::vector<NavigationEntry*>* entries);

  // Active entry --------------------------------------------------------------

  // Returns the active entry, which is the transient entry if any, the pending
  // entry if a navigation is in progress or the last committed entry otherwise.
  // NOTE: This can be NULL!!
  //
  // If you are trying to get the current state of the NavigationController,
  // this is the method you will typically want to call.  If you want to display
  // the active entry to the user (e.g., in the location bar), use
  // GetVisibleEntry instead.
  NavigationEntry* GetActiveEntry() const;

  // Returns the same entry as GetActiveEntry, except that it ignores pending
  // history navigation entries.  This should be used when displaying info to
  // the user, so that the location bar and other indicators do not update for
  // a back/forward navigation until the pending entry commits.  This approach
  // guards against URL spoofs on slow history navigations.
  NavigationEntry* GetVisibleEntry() const;

  // Returns the index from which we would go back/forward or reload.  This is
  // the last_committed_entry_index_ if pending_entry_index_ is -1.  Otherwise,
  // it is the pending_entry_index_.
  int GetCurrentEntryIndex() const;

  // Returns the last committed entry, which may be null if there are no
  // committed entries.
  NavigationEntry* GetLastCommittedEntry() const;

  // Returns true if the source for the current entry can be viewed.
  bool CanViewSource() const;

  // Returns the index of the last committed entry.
  int last_committed_entry_index() const {
    return last_committed_entry_index_;
  }

  // Navigation list -----------------------------------------------------------

  // Returns the number of entries in the NavigationController, excluding
  // the pending entry if there is one, but including the transient entry if
  // any.
  int entry_count() const {
    return static_cast<int>(entries_.size());
  }

  NavigationEntry* GetEntryAtIndex(int index) const {
    return entries_.at(index).get();
  }

  // Returns the entry at the specified offset from current.  Returns NULL
  // if out of bounds.
  NavigationEntry* GetEntryAtOffset(int offset) const;

  // Returns the index of the specified entry, or -1 if entry is not contained
  // in this NavigationController.
  int GetIndexOfEntry(const NavigationEntry* entry) const;

  // Return the index of the entry with the corresponding instance and page_id,
  // or -1 if not found.
  int GetEntryIndexWithPageID(SiteInstance* instance,
                              int32 page_id) const;

  // Return the entry with the corresponding instance and page_id, or NULL if
  // not found.
  NavigationEntry* GetEntryWithPageID(SiteInstance* instance,
                                      int32 page_id) const;

  // Pending entry -------------------------------------------------------------

  // Discards the pending and transient entries if any.
  void DiscardNonCommittedEntries();

  // Returns the pending entry corresponding to the navigation that is
  // currently in progress, or null if there is none.
  NavigationEntry* pending_entry() const {
    return pending_entry_;
  }

  // Returns the index of the pending entry or -1 if the pending entry
  // corresponds to a new navigation (created via LoadURL).
  int pending_entry_index() const {
    return pending_entry_index_;
  }

  // Transient entry -----------------------------------------------------------

  // Adds an entry that is returned by GetActiveEntry().  The entry is
  // transient: any navigation causes it to be removed and discarded.
  // The NavigationController becomes the owner of |entry| and deletes it when
  // it discards it.  This is useful with interstitial page that need to be
  // represented as an entry, but should go away when the user navigates away
  // from them.
  // Note that adding a transient entry does not change the active contents.
  void AddTransientEntry(NavigationEntry* entry);

  // Returns the transient entry if any.  Note that the returned entry is owned
  // by the navigation controller and may be deleted at any time.
  NavigationEntry* GetTransientEntry() const;

  // New navigations -----------------------------------------------------------

  // Loads the specified URL, specifying extra http headers to add to the
  // request.  Extra headers are separated by \n.
  void LoadURL(const GURL& url,
               const content::Referrer& referrer,
               content::PageTransition type,
               const std::string& extra_headers);

  // Same as LoadURL, but for renderer-initiated navigations.  This state is
  // important for tracking whether to display pending URLs.
  void LoadURLFromRenderer(const GURL& url,
                           const content::Referrer& referrer,
                           content::PageTransition type,
                           const std::string& extra_headers);

  // Behaves like LoadURL() and LoadURLFromRenderer() but marks the new
  // navigation as being transferred from one RVH to another. In this case the
  // browser can recycle the old request once the new renderer wants to
  // navigate.
  // |transferred_global_request_id| identifies the request ID of the old
  // request.
  void TransferURL(
      const GURL& url,
      const content::Referrer& referrer,
      content::PageTransition transition,
      const std::string& extra_headers,
      const GlobalRequestID& transferred_global_request_id,
      bool is_renderer_initiated);

  // Loads the current page if this NavigationController was restored from
  // history and the current page has not loaded yet.
  void LoadIfNecessary();

  // Renavigation --------------------------------------------------------------

  // Navigation relative to the "current entry"
  bool CanGoBack() const;
  bool CanGoForward() const;
  void GoBack();
  void GoForward();

  // Navigates to the specified absolute index.
  void GoToIndex(int index);

  // Navigates to the specified offset from the "current entry". Does nothing if
  // the offset is out of bounds.
  void GoToOffset(int offset);

  // Reloads the current entry. If |check_for_repost| is true and the current
  // entry has POST data the user is prompted to see if they really want to
  // reload the page. In nearly all cases pass in true.
  void Reload(bool check_for_repost);
  // Like Reload(), but don't use caches (aka "shift-reload").
  void ReloadIgnoringCache(bool check_for_repost);

  // Removing of entries -------------------------------------------------------

  // Removes the entry at the specified |index|.  This call dicards any pending
  // and transient entries.  |default_url| is the URL that the navigation
  // controller navigates to if there are no more entries after the removal.
  // If |default_url| is empty, we default to "about:blank".
  void RemoveEntryAtIndex(int index, const GURL& default_url);

  // TabContents ---------------------------------------------------------------

  // Returns the tab contents associated with this controller. Non-NULL except
  // during set-up of the tab.
  TabContents* tab_contents() const {
    // This currently returns the active tab contents which should be renamed to
    // tab_contents.
    return tab_contents_;
  }

  // Called when a document has been loaded in a frame.
  void DocumentLoadedInFrame();

  // For use by TabContents ----------------------------------------------------

  // Handles updating the navigation state after the renderer has navigated.
  // This is used by the TabContents.
  //
  // If a new entry is created, it will return true and will have filled the
  // given details structure and broadcast the NOTIFY_NAV_ENTRY_COMMITTED
  // notification. The caller can then use the details without worrying about
  // listening for the notification.
  //
  // In the case that nothing has changed, the details structure is undefined
  // and it will return false.
  bool RendererDidNavigate(const ViewHostMsg_FrameNavigate_Params& params,
                           content::LoadCommittedDetails* details);

  // Notifies us that we just became active. This is used by the TabContents
  // so that we know to load URLs that were pending as "lazy" loads.
  void SetActive(bool is_active);

  // Broadcasts the NOTIFY_NAV_ENTRY_CHANGED notification for the given entry
  // (which must be at the given index). This will keep things in sync like
  // the saved session.
  void NotifyEntryChanged(const NavigationEntry* entry, int index);

  // Returns true if the given URL would be an in-page navigation (i.e. only
  // the reference fragment is different) from the "last committed entry". We do
  // not compare it against the "active entry" since the active entry can be
  // pending and in page navigations only happen on committed pages. If there
  // is no last committed entry, then nothing will be in-page.
  //
  // Special note: if the URLs are the same, it does NOT count as an in-page
  // navigation. Neither does an input URL that has no ref, even if the rest is
  // the same. This may seem weird, but when we're considering whether a
  // navigation happened without loading anything, the same URL would be a
  // reload, while only a different ref would be in-page (pages can't clear
  // refs without reload, only change to "#" which we don't count as empty).
  bool IsURLInPageNavigation(const GURL& url) const;

  // Copies the navigation state from the given controller to this one. This
  // one should be empty (just created).
  void CopyStateFrom(const NavigationController& source);

  // A variant of CopyStateFrom. Removes all entries from this except the last
  // entry, inserts all entries from |source| before and including the active
  // entry. This method is intended for use when the last entry of |this| is the
  // active entry. For example:
  // source: A B *C* D
  // this:   E F *G*   (last must be active or pending)
  // result: A B *G*
  // This ignores the transient index of the source and honors that of 'this'.
  void CopyStateFromAndPrune(NavigationController* source);

  // Removes all the entries except the active entry. If there is a new pending
  // navigation it is preserved.
  void PruneAllButActive();

  // Random data ---------------------------------------------------------------

  SSLManager* ssl_manager() { return &ssl_manager_; }

  // Returns true if a reload happens when activated (SetActive(true) is
  // invoked). This is true for session/tab restore and cloned tabs.
  bool needs_reload() const { return needs_reload_; }

  // Sets the max restored page ID this NavigationController has seen, if it
  // was restored from a previous session.
  void set_max_restored_page_id(int32 max_id) {
    max_restored_page_id_ = max_id;
  }

  // Returns the largest restored page ID seen in this navigation controller,
  // if it was restored from a previous session.  (-1 otherwise)
  int32 max_restored_page_id() const { return max_restored_page_id_; }

  // The session storage namespace that all child render views should use.
  SessionStorageNamespace* session_storage_namespace() const {
    return session_storage_namespace_;
  }

  // Disables checking for a repost and prompting the user. This is used during
  // testing.
  static void DisablePromptOnRepost();

  // Maximum number of entries before we start removing entries from the front.
  static void set_max_entry_count_for_testing(size_t max_entry_count) {
    max_entry_count_ = max_entry_count;
  }
  static size_t max_entry_count() { return max_entry_count_; }

  // Cancels a repost that brought up a warning.
  void CancelPendingReload();
  // Continues a repost that brought up a warning.
  void ContinuePendingReload();

  // Returns true if we are navigating to the URL the tab is opened with.
  bool IsInitialNavigation();

  // Creates navigation entry and translates the virtual url to a real one.
  // Used when navigating to a new URL using LoadURL.  Extra headers are
  // separated by \n.
  static NavigationEntry* CreateNavigationEntry(
      const GURL& url,
      const content::Referrer& referrer,
      content::PageTransition transition,
      bool is_renderer_initiated,
      const std::string& extra_headers,
      content::BrowserContext* browser_context);

 private:
  class RestoreHelper;
  friend class RestoreHelper;
  friend class TabContents;  // For invoking OnReservedPageIDRange.

  // Classifies the given renderer navigation (see the NavigationType enum).
  content::NavigationType ClassifyNavigation(
      const ViewHostMsg_FrameNavigate_Params& params) const;

  // Causes the controller to load the specified entry. The function assumes
  // ownership of the pointer since it is put in the navigation list.
  // NOTE: Do not pass an entry that the controller already owns!
  void LoadEntry(NavigationEntry* entry);

  // Handlers for the different types of navigation types. They will actually
  // handle the navigations corresponding to the different NavClasses above.
  // They will NOT broadcast the commit notification, that should be handled by
  // the caller.
  //
  // RendererDidNavigateAutoSubframe is special, it may not actually change
  // anything if some random subframe is loaded. It will return true if anything
  // changed, or false if not.
  //
  // The functions taking |did_replace_entry| will fill into the given variable
  // whether the last entry has been replaced or not.
  // See LoadCommittedDetails.did_replace_entry.
  void RendererDidNavigateToNewPage(
      const ViewHostMsg_FrameNavigate_Params& params, bool* did_replace_entry);
  void RendererDidNavigateToExistingPage(
      const ViewHostMsg_FrameNavigate_Params& params);
  void RendererDidNavigateToSamePage(
      const ViewHostMsg_FrameNavigate_Params& params);
  void RendererDidNavigateInPage(
      const ViewHostMsg_FrameNavigate_Params& params, bool* did_replace_entry);
  void RendererDidNavigateNewSubframe(
      const ViewHostMsg_FrameNavigate_Params& params);
  bool RendererDidNavigateAutoSubframe(
      const ViewHostMsg_FrameNavigate_Params& params);

  // Helper function for code shared between Reload() and ReloadIgnoringCache().
  void ReloadInternal(bool check_for_repost, ReloadType reload_type);

  // Actually issues the navigation held in pending_entry.
  void NavigateToPendingEntry(ReloadType reload_type);

  // Allows the derived class to issue notifications that a load has been
  // committed. This will fill in the active entry to the details structure.
  void NotifyNavigationEntryCommitted(content::LoadCommittedDetails* details);

  // Updates the virtual URL of an entry to match a new URL, for cases where
  // the real renderer URL is derived from the virtual URL, like view-source:
  void UpdateVirtualURLToURL(NavigationEntry* entry, const GURL& new_url);

  // Invoked after session/tab restore or cloning a tab. Resets the transition
  // type of the entries, updates the max page id and creates the active
  // contents. See RestoreFromState for a description of from_last_session.
  void FinishRestore(int selected_index, bool from_last_session);

  // Inserts a new entry or replaces the current entry with a new one, removing
  // all entries after it. The new entry will become the active one.
  void InsertOrReplaceEntry(NavigationEntry* entry, bool replace);

  // Removes the entry at |index|.
  void RemoveEntryAtIndexInternal(int index);

  // Discards the pending and transient entries.
  void DiscardNonCommittedEntriesInternal();

  // Discards the transient entry.
  void DiscardTransientEntry();

  // Returns true if the navigation is redirect.
  bool IsRedirect(const ViewHostMsg_FrameNavigate_Params& params);

  // Returns true if the navigation is likley to be automatic rather than
  // user-initiated.
  bool IsLikelyAutoNavigation(base::TimeTicks now);

  // Inserts up to |max_index| entries from |source| into this. This does NOT
  // adjust any of the members that reference entries_
  // (last_committed_entry_index_, pending_entry_index_ or
  // transient_entry_index_).
  void InsertEntriesFrom(const NavigationController& source, int max_index);

  // ---------------------------------------------------------------------------

  // The user browser context associated with this controller.
  content::BrowserContext* browser_context_;

  // List of NavigationEntry for this tab
  typedef std::vector<linked_ptr<NavigationEntry> > NavigationEntries;
  NavigationEntries entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  //
  // This may refer to an item in the entries_ list if the pending_entry_index_
  // == -1, or it may be its own entry that should be deleted. Be careful with
  // the memory management.
  NavigationEntry* pending_entry_;

  // currently visible entry
  int last_committed_entry_index_;

  // index of pending entry if it is in entries_, or -1 if pending_entry_ is a
  // new entry (created by LoadURL).
  int pending_entry_index_;

  // The index for the entry that is shown until a navigation occurs.  This is
  // used for interstitial pages. -1 if there are no such entry.
  // Note that this entry really appears in the list of entries, but only
  // temporarily (until the next navigation).  Any index pointing to an entry
  // after the transient entry will become invalid if you navigate forward.
  int transient_entry_index_;

  // The tab contents associated with the controller. Possibly NULL during
  // setup.
  TabContents* tab_contents_;

  // The max restored page ID in this controller, if it was restored.  We must
  // store this so that TabContents can tell any renderer in charge of one of
  // the restored entries to update its max page ID.
  int32 max_restored_page_id_;

  // Manages the SSL security UI
  SSLManager ssl_manager_;

  // Whether we need to be reloaded when made active.
  bool needs_reload_;

  // The time ticks at which the last document was loaded.
  base::TimeTicks last_document_loaded_;

  // The session storage id that any (indirectly) owned RenderView should use.
  scoped_refptr<SessionStorageNamespace> session_storage_namespace_;

  // Should Reload check for post data? The default is true, but is set to false
  // when testing.
  static bool check_for_repost_;

  // The maximum number of entries that a navigation controller can store.
  static size_t max_entry_count_;

  // If a repost is pending, its type (RELOAD or RELOAD_IGNORING_CACHE),
  // NO_RELOAD otherwise.
  ReloadType pending_reload_;

  DISALLOW_COPY_AND_ASSIGN(NavigationController);
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_NAVIGATION_CONTROLLER_H_
