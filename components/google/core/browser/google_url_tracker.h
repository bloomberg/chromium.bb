// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_H_
#define COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_H_

#include <map>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/google/core/browser/google_url_tracker_client.h"
#include "components/google/core/browser/google_url_tracker_map_entry.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class GoogleURLTrackerNavigationHelper;
class PrefService;

namespace infobars {
class InfoBar;
}

// This object is responsible for checking the Google URL once per network
// change, and if necessary prompting the user to see if they want to change to
// using it.  The current and last prompted values are saved to prefs.
//
// Most consumers should only call google_url().  Consumers who need to be
// notified when things change should register a callback that provides the
// original and updated values via RegisterCallback().
//
// To protect users' privacy and reduce server load, no updates will be
// performed (ever) unless at least one consumer registers interest by calling
// RequestServerCheck().
class GoogleURLTracker
    : public net::URLFetcherDelegate,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public KeyedService {
 public:
  // Callback that is called when the Google URL is updated. The arguments are
  // the old and new URLs.
  typedef base::Callback<void()> OnGoogleURLUpdatedCallback;
  typedef base::CallbackList<void()> CallbackList;
  typedef CallbackList::Subscription Subscription;

  // The constructor does different things depending on which of these values
  // you pass it.  Hopefully these are self-explanatory.
  enum Mode {
    NORMAL_MODE,
    UNIT_TEST_MODE,
  };

  static const char kDefaultGoogleHomepage[];

  // Only the GoogleURLTrackerFactory and tests should call this.
  GoogleURLTracker(scoped_ptr<GoogleURLTrackerClient> client, Mode mode);

  virtual ~GoogleURLTracker();

  // Returns the current Google homepage URL.
  const GURL& google_url() const { return google_url_; }

  // Requests that the tracker perform a server check to update the Google URL
  // as necessary.  If |force| is false, this will happen at most once per
  // network change, not sooner than five seconds after startup (checks
  // requested before that time will occur then; checks requested afterwards
  // will occur immediately, if no other checks have been made during this run).
  // If |force| is true, and the tracker has already performed any requested
  // check, it will check again.
  void RequestServerCheck(bool force);

  // Notifies the tracker that the user has started a Google search.
  // If prompting is necessary, we then listen for the subsequent pending
  // navigation to get the appropriate NavigationHelper. When the load
  // commits, we'll show the infobar.
  void SearchCommitted();

  // No one but GoogleURLTrackerInfoBarDelegate or test code should call these.
  void AcceptGoogleURL(bool redo_searches);
  void CancelGoogleURL();
  const GURL& fetched_google_url() const { return fetched_google_url_; }
  GoogleURLTrackerClient* client() { return client_.get(); }

  // No one but GoogleURLTrackerMapEntry should call this.
  void DeleteMapEntryForManager(
      const infobars::InfoBarManager* infobar_manager);

  // Called by the client after SearchCommitted() registers listeners,
  // to indicate that we've received the "load now pending" notification.
  // |nav_helper| is the GoogleURLTrackerNavigationHelper associated with this
  // navigation; |infobar_manager| is the InfoBarManager of the associated tab;
  // and |pending_id| is the unique ID of the newly pending NavigationEntry.
  // If there is already a visible GoogleURLTracker infobar for this tab, this
  // function resets its associated pending entry ID to the new ID.  Otherwise
  // this function creates a map entry for the associated tab.
  virtual void OnNavigationPending(
      scoped_ptr<GoogleURLTrackerNavigationHelper> nav_helper,
      infobars::InfoBarManager* infobar_manager,
      int pending_id);

  // Called by the navigation observer once a load we're watching commits.
  // |infobar_manager| is the same as for OnNavigationPending();
  // |search_url| is guaranteed to be valid.
  virtual void OnNavigationCommitted(infobars::InfoBarManager* infobar_manager,
                                     const GURL& search_url);

  // Called by the navigation observer when a tab closes.
  virtual void OnTabClosed(GoogleURLTrackerNavigationHelper* nav_helper);

  scoped_ptr<Subscription> RegisterCallback(
      const OnGoogleURLUpdatedCallback& cb);

 private:
  friend class GoogleURLTrackerTest;
  friend class SyncTest;

  typedef std::map<const infobars::InfoBarManager*, GoogleURLTrackerMapEntry*>
      EntryMap;

  static const char kSearchDomainCheckURL[];

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  // Registers consumer interest in getting an updated URL from the server.
  // Observe chrome::NOTIFICATION_GOOGLE_URL_UPDATED to be notified when the URL
  // changes.
  void SetNeedToFetch();

  // Called when the five second startup sleep has finished.  Runs any pending
  // fetch.
  void FinishSleep();

  // Starts the fetch of the up-to-date Google URL if we actually want to fetch
  // it and can currently do so.
  void StartFetchIfDesirable();

  // Closes all map entries.  If |redo_searches| is true, this also triggers
  // each tab with an infobar to re-perform the user's search, but on the new
  // Google TLD.
  void CloseAllEntries(bool redo_searches);

  // Unregisters any listeners for the navigation helper in |map_entry|.
  // This sanity-DCHECKs that these are registered (or not) in the specific
  // cases we expect.  (|must_be_listening_for_commit| is used purely for this
  // sanity-checking.)  This also unregisters the global navigation pending
  // listener if there are no remaining listeners for navigation commits, as we
  // no longer need them until another search is committed.
  void UnregisterForEntrySpecificNotifications(
      GoogleURLTrackerMapEntry* map_entry,
      bool must_be_listening_for_commit);

  void NotifyGoogleURLUpdated();

  CallbackList callback_list_;

  scoped_ptr<GoogleURLTrackerClient> client_;

  GURL google_url_;
  GURL fetched_google_url_;
  scoped_ptr<net::URLFetcher> fetcher_;
  int fetcher_id_;
  bool in_startup_sleep_;  // True if we're in the five-second "no fetching"
                           // period that begins at browser start.
  bool already_fetched_;   // True if we've already fetched a URL once this run;
                           // we won't fetch again until after a restart.
  bool need_to_fetch_;     // True if a consumer actually wants us to fetch an
                           // updated URL.  If this is never set, we won't
                           // bother to fetch anything.
                           // Consumers should register a callback via
                           // RegisterCallback().
  bool need_to_prompt_;    // True if the last fetched Google URL is not
                           // matched with current user's default Google URL
                           // nor the last prompted Google URL.
  bool search_committed_;  // True when we're expecting a notification of a new
                           // pending search navigation.
  EntryMap entry_map_;
  base::WeakPtrFactory<GoogleURLTracker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTracker);
};

#endif  // COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_H_
