// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"

namespace base {

class RefCountedMemory;

}  // namespace base

namespace content {

class BrowserContext;
class NavigationEntry;
class SessionStorageNamespace;
class WebContents;

// Used to store the mapping of a StoragePartition id to
// SessionStorageNamespace.
typedef std::map<std::string, scoped_refptr<SessionStorageNamespace> >
    SessionStorageNamespaceMap;

// A NavigationController maintains the back-forward list for a WebContents and
// manages all navigation within that list.
//
// Each NavigationController belongs to one WebContents; each WebContents has
// exactly one NavigationController.
class NavigationController {
 public:
  enum ReloadType {
    NO_RELOAD,                   // Normal load.
    RELOAD,                      // Normal (cache-validating) reload.
    RELOAD_IGNORING_CACHE,       // Reload bypassing the cache (shift-reload).
    RELOAD_ORIGINAL_REQUEST_URL  // Reload using the original request URL.
  };

  // Load type used in LoadURLParams.
  enum LoadURLType {
    // For loads that do not fall into any types below.
    LOAD_TYPE_DEFAULT,

    // An http post load request initiated from browser side.
    // The post data is passed in |browser_initiated_post_data|.
    LOAD_TYPE_BROWSER_INITIATED_HTTP_POST,

    // Loads a 'data:' scheme URL with specified base URL and a history entry
    // URL. This is only safe to be used for browser-initiated data: URL
    // navigations, since it shows arbitrary content as if it comes from
    // |virtual_url_for_data_url|.
    LOAD_TYPE_DATA

    // Adding new LoadURLType? Also update LoadUrlParams.java static constants.
  };

  // User agent override type used in LoadURLParams.
  enum UserAgentOverrideOption {
    // Use the override value from the previous NavigationEntry in the
    // NavigationController.
    UA_OVERRIDE_INHERIT,

    // Use the default user agent.
    UA_OVERRIDE_FALSE,

    // Use the user agent override, if it's available.
    UA_OVERRIDE_TRUE

    // Adding new UserAgentOverrideOption? Also update LoadUrlParams.java
    // static constants.
  };

  enum RestoreType {
    // Indicates the restore is from the current session. For example, restoring
    // a closed tab.
    RESTORE_CURRENT_SESSION,

    // Restore from the previous session.
    RESTORE_LAST_SESSION_EXITED_CLEANLY,
    RESTORE_LAST_SESSION_CRASHED,
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

  // Extra optional parameters for LoadURLWithParams.
  struct CONTENT_EXPORT LoadURLParams {
    // The url to load. This field is required.
    GURL url;

    // See LoadURLType comments above.
    LoadURLType load_type;

    // PageTransition for this load. See PageTransition for details.
    // Note the default value in constructor below.
    PageTransition transition_type;

    // Referrer for this load. Empty if none.
    Referrer referrer;

    // Extra headers for this load, separated by \n.
    std::string extra_headers;

    // True for renderer-initiated navigations. This is
    // important for tracking whether to display pending URLs.
    bool is_renderer_initiated;

    // User agent override for this load. See comments in
    // UserAgentOverrideOption definition.
    UserAgentOverrideOption override_user_agent;

    // Marks the new navigation as being transferred from one RVH to another.
    // In this case the browser can recycle the old request once the new
    // renderer wants to navigate. Identifies the request ID of the old request.
    GlobalRequestID transferred_global_request_id;

    // Used in LOAD_TYPE_DATA loads only. Used for specifying a base URL
    // for pages loaded via data URLs.
    GURL base_url_for_data_url;

    // Used in LOAD_TYPE_DATA loads only. URL displayed to the user for
    // data loads.
    GURL virtual_url_for_data_url;

    // Used in LOAD_TYPE_BROWSER_INITIATED_HTTP_POST loads only. Carries the
    // post data of the load. Ownership is transferred to NavigationController
    // after LoadURLWithParams call.
    scoped_refptr<base::RefCountedMemory> browser_initiated_post_data;

    // True if this URL should be able to access local resources.
    bool can_load_local_resources;

    // Indicates whether this navigation involves a cross-process redirect,
    // in which case it should replace the current navigation entry.
    bool is_cross_site_redirect;

    explicit LoadURLParams(const GURL& url);
    ~LoadURLParams();

    // Allows copying of LoadURLParams struct.
    LoadURLParams(const LoadURLParams& other);
    LoadURLParams& operator=(const LoadURLParams& other);
  };

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
  // using |selected_navigation| as the currently loaded entry. Before this call
  // the controller should be unused (there should be no current entry). |type|
  // indicates where the restor comes from. This takes ownership of the
  // NavigationEntrys in |entries| and clears it out.  This is used for session
  // restore.
  virtual void Restore(int selected_navigation,
                       RestoreType type,
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

  // Adds an entry that is returned by GetActiveEntry(). The entry is
  // transient: any navigation causes it to be removed and discarded.  The
  // NavigationController becomes the owner of |entry| and deletes it when
  // it discards it. This is useful with interstitial pages that need to be
  // represented as an entry, but should go away when the user navigates away
  // from them.
  // Note that adding a transient entry does not change the active contents.
  virtual void SetTransientEntry(NavigationEntry* entry) = 0;

  // New navigations -----------------------------------------------------------

  // Loads the specified URL, specifying extra http headers to add to the
  // request.  Extra headers are separated by \n.
  virtual void LoadURL(const GURL& url,
                       const Referrer& referrer,
                       PageTransition type,
                       const std::string& extra_headers) = 0;

  // More general version of LoadURL. See comments in LoadURLParams for
  // using |params|.
  virtual void LoadURLWithParams(const LoadURLParams& params) = 0;

  // Loads the current page if this NavigationController was restored from
  // history and the current page has not loaded yet.
  virtual void LoadIfNecessary() = 0;

  // Renavigation --------------------------------------------------------------

  // Navigation relative to the "current entry"
  virtual bool CanGoBack() const = 0;
  virtual bool CanGoForward() const = 0;
  virtual bool CanGoToOffset(int offset) const = 0;
  virtual void GoBack() = 0;
  virtual void GoForward() = 0;

  // Navigates to the specified absolute index.
  virtual void GoToIndex(int index) = 0;

  // Navigates to the specified offset from the "current entry". Does nothing if
  // the offset is out of bounds.
  virtual void GoToOffset(int offset) = 0;

  // Reloads the current entry. If |check_for_repost| is true and the current
  // entry has POST data the user is prompted to see if they really want to
  // reload the page. In nearly all cases pass in true.  If a transient entry
  // is showing, initiates a new navigation to its URL.
  virtual void Reload(bool check_for_repost) = 0;

  // Like Reload(), but don't use caches (aka "shift-reload").
  virtual void ReloadIgnoringCache(bool check_for_repost) = 0;

  // Reloads the current entry using the original URL used to create it.  This
  // is used for cases where the user wants to refresh a page using a different
  // user agent after following a redirect.
  virtual void ReloadOriginalRequestURL(bool check_for_repost) = 0;

  // Removing of entries -------------------------------------------------------

  // Removes the entry at the specified |index|.  This call dicards any pending
  // and transient entries.  If the index is the last committed index, this does
  // nothing and returns false.
  virtual void RemoveEntryAtIndex(int index) = 0;

  // Random --------------------------------------------------------------------

  // Session storage depends on dom_storage that depends on WebKit::WebString,
  // which cannot be used on iOS.
#if !defined(OS_IOS)
  // Returns all the SessionStorageNamespace objects that this
  // NavigationController knows about.
  virtual const SessionStorageNamespaceMap&
      GetSessionStorageNamespaceMap() const = 0;

  // TODO(ajwong): Remove this once prerendering, instant, and session restore
  // are migrated.
  virtual SessionStorageNamespace* GetDefaultSessionStorageNamespace() = 0;
#endif

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
  // Returns false after initial navigation has loaded in frame.
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

  // Clears all screenshots associated with navigation entries in this
  // controller. Useful to reduce memory consumption in low-memory situations.
  virtual void ClearAllScreenshots() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_CONTROLLER_H_
