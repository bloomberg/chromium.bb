// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_NAVIGATION_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_WEB_CONTENTS_NAVIGATION_CONTROLLER_IMPL_H_
#pragma once

#include "build/build_config.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/time.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_type.h"

class SessionStorageNamespaceImpl;
struct ViewHostMsg_FrameNavigate_Params;
class WebContentsImpl;

namespace content {
class NavigationEntryImpl;
struct LoadCommittedDetails;
class SiteInstance;
}

class CONTENT_EXPORT NavigationControllerImpl
    : public NON_EXPORTED_BASE(content::NavigationController) {
 public:
  NavigationControllerImpl(
      WebContentsImpl* web_contents,
      content::BrowserContext* browser_context,
      SessionStorageNamespaceImpl* session_storage_namespace);
  virtual ~NavigationControllerImpl();

  // NavigationController implementation:
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual void SetBrowserContext(
      content::BrowserContext* browser_context) OVERRIDE;
  virtual void Restore(
      int selected_navigation,
      bool from_last_session,
      std::vector<content::NavigationEntry*>* entries) OVERRIDE;
  virtual content::NavigationEntry* GetActiveEntry() const OVERRIDE;
  virtual content::NavigationEntry* GetVisibleEntry() const OVERRIDE;
  virtual int GetCurrentEntryIndex() const OVERRIDE;
  virtual content::NavigationEntry* GetLastCommittedEntry() const OVERRIDE;
  virtual int GetLastCommittedEntryIndex() const OVERRIDE;
  virtual bool CanViewSource() const OVERRIDE;
  virtual int GetEntryCount() const OVERRIDE;
  virtual content::NavigationEntry* GetEntryAtIndex(int index) const OVERRIDE;
  virtual content::NavigationEntry* GetEntryAtOffset(int offset) const OVERRIDE;
  virtual void DiscardNonCommittedEntries() OVERRIDE;
  virtual content::NavigationEntry* GetPendingEntry() const OVERRIDE;
  virtual int GetPendingEntryIndex() const OVERRIDE;
  virtual content::NavigationEntry* GetTransientEntry() const OVERRIDE;
  virtual void LoadURL(const GURL& url,
                       const content::Referrer& referrer,
                       content::PageTransition type,
                       const std::string& extra_headers) OVERRIDE;
  virtual void LoadURLFromRenderer(const GURL& url,
                                   const content::Referrer& referrer,
                                   content::PageTransition type,
                                   const std::string& extra_headers) OVERRIDE;
  virtual void LoadURLWithUserAgentOverride(
      const GURL& url,
      const content::Referrer& referrer,
      content::PageTransition type,
      bool is_renderer_initiated,
      const std::string& extra_headers,
      bool is_overriding_user_agent) OVERRIDE;
  virtual void TransferURL(
      const GURL& url,
      const content::Referrer& referrer,
      content::PageTransition transition,
      const std::string& extra_headers,
      const content::GlobalRequestID& transferred_global_request_id,
      bool is_renderer_initiated) OVERRIDE;
  virtual void LoadIfNecessary() OVERRIDE;
  virtual bool CanGoBack() const OVERRIDE;
  virtual bool CanGoForward() const OVERRIDE;
  virtual void GoBack() OVERRIDE;
  virtual void GoForward() OVERRIDE;
  virtual void GoToIndex(int index) OVERRIDE;
  virtual void GoToOffset(int offset) OVERRIDE;
  virtual void RemoveEntryAtIndex(int index) OVERRIDE;
  virtual content::SessionStorageNamespace*
      GetSessionStorageNamespace() const OVERRIDE;
  virtual void SetMaxRestoredPageID(int32 max_id) OVERRIDE;
  virtual int32 GetMaxRestoredPageID() const OVERRIDE;
  virtual bool NeedsReload() const OVERRIDE;
  virtual void CancelPendingReload() OVERRIDE;
  virtual void ContinuePendingReload() OVERRIDE;
  virtual bool IsInitialNavigation() OVERRIDE;
  virtual void Reload(bool check_for_repost) OVERRIDE;
  virtual void ReloadIgnoringCache(bool check_for_repost) OVERRIDE;
  virtual void NotifyEntryChanged(const content::NavigationEntry* entry,
                                 int index) OVERRIDE;
  virtual void CopyStateFrom(
      const content::NavigationController& source) OVERRIDE;
  virtual void CopyStateFromAndPrune(
      content::NavigationController* source) OVERRIDE;
  virtual void PruneAllButActive() OVERRIDE;

  // Returns the index of the specified entry, or -1 if entry is not contained
  // in this NavigationController.
  int GetIndexOfEntry(const content::NavigationEntryImpl* entry) const;

  // Return the index of the entry with the corresponding instance and page_id,
  // or -1 if not found.
  int GetEntryIndexWithPageID(content::SiteInstance* instance,
                              int32 page_id) const;

  // Return the entry with the corresponding instance and page_id, or NULL if
  // not found.
  content::NavigationEntryImpl* GetEntryWithPageID(
      content::SiteInstance* instance,
      int32 page_id) const;

  // Transient entry -----------------------------------------------------------

  // Adds an entry that is returned by GetActiveEntry().  The entry is
  // transient: any navigation causes it to be removed and discarded.
  // The NavigationController becomes the owner of |entry| and deletes it when
  // it discards it.  This is useful with interstitial page that need to be
  // represented as an entry, but should go away when the user navigates away
  // from them.
  // Note that adding a transient entry does not change the active contents.
  void AddTransientEntry(content::NavigationEntryImpl* entry);

  // WebContentsImpl -----------------------------------------------------------

  WebContentsImpl* web_contents() const {
    return web_contents_;
  }

  // Called when a document has been loaded in a frame.
  void DocumentLoadedInFrame();

  // For use by WebContentsImpl ------------------------------------------------

  // Handles updating the navigation state after the renderer has navigated.
  // This is used by the WebContentsImpl.
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

  // Notifies us that we just became active. This is used by the WebContentsImpl
  // so that we know to load URLs that were pending as "lazy" loads.
  void SetActive(bool is_active);

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

  // Random data ---------------------------------------------------------------

  SSLManager* ssl_manager() { return &ssl_manager_; }

  // Maximum number of entries before we start removing entries from the front.
  static void set_max_entry_count_for_testing(size_t max_entry_count) {
    max_entry_count_for_testing_ = max_entry_count;
  }
  static size_t max_entry_count();

 private:
  class RestoreHelper;
  friend class RestoreHelper;
  friend class WebContentsImpl;  // For invoking OnReservedPageIDRange.

  // Classifies the given renderer navigation (see the NavigationType enum).
  content::NavigationType ClassifyNavigation(
      const ViewHostMsg_FrameNavigate_Params& params) const;

  // Causes the controller to load the specified entry. The function assumes
  // ownership of the pointer since it is put in the navigation list.
  // NOTE: Do not pass an entry that the controller already owns!
  void LoadEntry(content::NavigationEntryImpl* entry);

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
  void UpdateVirtualURLToURL(content::NavigationEntryImpl* entry,
                             const GURL& new_url);

  // Invoked after session/tab restore or cloning a tab. Resets the transition
  // type of the entries, updates the max page id and creates the active
  // contents. See RestoreFromState for a description of from_last_session.
  void FinishRestore(int selected_index, bool from_last_session);

  // Inserts a new entry or replaces the current entry with a new one, removing
  // all entries after it. The new entry will become the active one.
  void InsertOrReplaceEntry(content::NavigationEntryImpl* entry, bool replace);

  // Removes the entry at |index|, as long as it is not the current entry.
  void RemoveEntryAtIndexInternal(int index);

  // Discards the pending and transient entries.
  void DiscardNonCommittedEntriesInternal();

  // Discards the transient entry.
  void DiscardTransientEntry();

  // If we have the maximum number of entries, remove the oldest one in
  // preparation to add another.
  void PruneOldestEntryIfFull();

  // Returns true if the navigation is redirect.
  bool IsRedirect(const ViewHostMsg_FrameNavigate_Params& params);

  // Returns true if the navigation is likley to be automatic rather than
  // user-initiated.
  bool IsLikelyAutoNavigation(base::TimeTicks now);

  // Inserts up to |max_index| entries from |source| into this. This does NOT
  // adjust any of the members that reference entries_
  // (last_committed_entry_index_, pending_entry_index_ or
  // transient_entry_index_).
  void InsertEntriesFrom(const NavigationControllerImpl& source, int max_index);

  // ---------------------------------------------------------------------------

  // The user browser context associated with this controller.
  content::BrowserContext* browser_context_;

  // List of NavigationEntry for this tab
  typedef std::vector<linked_ptr<content::NavigationEntryImpl> >
      NavigationEntries;
  NavigationEntries entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  //
  // This may refer to an item in the entries_ list if the pending_entry_index_
  // == -1, or it may be its own entry that should be deleted. Be careful with
  // the memory management.
  content::NavigationEntryImpl* pending_entry_;

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

  // The WebContents associated with the controller. Possibly NULL during
  // setup.
  WebContentsImpl* web_contents_;

  // The max restored page ID in this controller, if it was restored.  We must
  // store this so that WebContentsImpl can tell any renderer in charge of one
  // of the restored entries to update its max page ID.
  int32 max_restored_page_id_;

  // Manages the SSL security UI
  SSLManager ssl_manager_;

  // Whether we need to be reloaded when made active.
  bool needs_reload_;

  // The time ticks at which the last document was loaded.
  base::TimeTicks last_document_loaded_;

  // The session storage id that any (indirectly) owned RenderView should use.
  scoped_refptr<SessionStorageNamespaceImpl> session_storage_namespace_;

  // The maximum number of entries that a navigation controller can store.
  static size_t max_entry_count_for_testing_;

  // If a repost is pending, its type (RELOAD or RELOAD_IGNORING_CACHE),
  // NO_RELOAD otherwise.
  ReloadType pending_reload_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerImpl);
};

#endif  // CONTENT_BROWSER_WEB_CONTENTS_NAVIGATION_CONTROLLER_IMPL_H_
