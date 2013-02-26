// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_

#include <map>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/google/google_url_tracker_map_entry.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

class GoogleURLTrackerNavigationHelper;
class PrefService;
class Profile;

namespace content {
class NavigationController;
}

// This object is responsible for checking the Google URL once per network
// change, and if necessary prompting the user to see if they want to change to
// using it.  The current and last prompted values are saved to prefs.
//
// Most consumers should only call GoogleURL(), which is guaranteed to
// synchronously return a value at all times (even during startup or in unittest
// mode).  Consumers who need to be notified when things change should listen to
// the notification service for NOTIFICATION_GOOGLE_URL_UPDATED, which provides
// the original and updated values.
//
// To protect users' privacy and reduce server load, no updates will be
// performed (ever) unless at least one consumer registers interest by calling
// RequestServerCheck().
class GoogleURLTracker : public net::URLFetcherDelegate,
                         public net::NetworkChangeNotifier::IPAddressObserver,
                         public ProfileKeyedService {
 public:
  // The contents of the Details for a NOTIFICATION_GOOGLE_URL_UPDATED.
  typedef std::pair<GURL, GURL> UpdatedDetails;

  // The constructor does different things depending on which of these values
  // you pass it.  Hopefully these are self-explanatory.
  enum Mode {
    NORMAL_MODE,
    UNIT_TEST_MODE,
  };

  // Only the GoogleURLTrackerFactory and tests should call this.  No code other
  // than the GoogleURLTracker itself should actually use
  // GoogleURLTrackerFactory::GetForProfile().
  GoogleURLTracker(Profile* profile,
                   scoped_ptr<GoogleURLTrackerNavigationHelper> nav_helper,
                   Mode mode);

  virtual ~GoogleURLTracker();

  // Returns the current Google URL.  This will return a valid URL even if
  // |profile| is NULL or a testing profile.
  //
  // This is the only function most code should ever call.
  static GURL GoogleURL(Profile* profile);

  // Requests that the tracker perform a server check to update the Google URL
  // as necessary.  This will happen at most once per network change, not
  // sooner than five seconds after startup (checks requested before that time
  // will occur then; checks requested afterwards will occur immediately, if
  // no other checks have been made during this run).
  //
  // When |profile| is NULL or a testing profile, this function does nothing.
  static void RequestServerCheck(Profile* profile);

  // Notifies the tracker that the user has started a Google search.
  // If prompting is necessary, we then listen for the subsequent pending
  // navigation to get the appropriate NavigationController. When the load
  // commits, we'll show the infobar.
  //
  // When |profile| is NULL or a testing profile, this function does nothing.
  static void GoogleURLSearchCommitted(Profile* profile);

  // No one but GoogleURLTrackerInfoBarDelegate or test code should call these.
  void AcceptGoogleURL(bool redo_searches);
  void CancelGoogleURL();
  const GURL& google_url() const { return google_url_; }
  const GURL& fetched_google_url() const { return fetched_google_url_; }

  // No one but GoogleURLTrackerMapEntry should call this.
  void DeleteMapEntryForService(const InfoBarService* infobar_service);

  // Called by the navigation observer after SearchCommitted() registers
  // listeners, to indicate that we've received the "load now pending"
  // notification.  |navigation_controller| is the NavigationController for this
  // load; |infobar_service| is the InfoBarService of the associated tab; and
  // |pending_id| is the unique ID of the newly pending NavigationEntry.
  // If there is already a visible GoogleURLTracker infobar for this tab, this
  // function resets its associated pending entry ID to the new ID.  Otherwise
  // this function creates a (still-invisible) InfoBarDelegate for the
  // associated tab.
  virtual void OnNavigationPending(
      content::NavigationController* navigation_controller,
      InfoBarService* infobar_service,
      int pending_id);

  // Called by the navigation observer once a load we're watching commits.
  // |infobar_service| is the same as for OnNavigationPending();
  // |search_url| is guaranteed to be valid.
  virtual void OnNavigationCommitted(InfoBarService* infobar_service,
                                     const GURL& search_url);

  // Called by the navigation observer when a tab closes.
  virtual void OnTabClosed(
      content::NavigationController* navigation_controller);

  static const char kDefaultGoogleHomepage[];
  static const char kSearchDomainCheckURL[];

 private:
  friend class GoogleURLTrackerTest;

  typedef std::map<const InfoBarService*, GoogleURLTrackerMapEntry*> EntryMap;

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  // ProfileKeyedService:
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

  // Called each time the user performs a search.  This checks whether we need
  // to prompt the user about a domain change, and if so, starts listening for
  // the notifications sent when the actual load is triggered.
  void SearchCommitted();

  // Closes all map entries.  If |redo_searches| is true, this also triggers
  // each tab with an infobar to re-perform the user's search, but on the new
  // Google TLD.
  void CloseAllEntries(bool redo_searches);

  // Unregisters any listeners for the navigation controller in |map_entry|.
  // This sanity-DCHECKs that these are registered (or not) in the specific
  // cases we expect.  (|must_be_listening_for_commit| is used purely for this
  // sanity-checking.)  This also unregisters the global navigation pending
  // listener if there are no remaining listeners for navigation commits, as we
  // no longer need them until another search is committed.
  void UnregisterForEntrySpecificNotifications(
      const GoogleURLTrackerMapEntry& map_entry,
      bool must_be_listening_for_commit);

  Profile* profile_;

  scoped_ptr<GoogleURLTrackerNavigationHelper> nav_helper_;

  // Creates an infobar delegate and adds it to the provided InfoBarService.
  // Returns the delegate pointer on success or NULL on failure.  The caller
  // does not own the returned object, the InfoBarService does.
  base::Callback<GoogleURLTrackerInfoBarDelegate*(
      InfoBarService*,
      GoogleURLTracker*,
      const GURL&)> infobar_creator_;

  GURL google_url_;
  GURL fetched_google_url_;
  base::WeakPtrFactory<GoogleURLTracker> weak_ptr_factory_;
  scoped_ptr<net::URLFetcher> fetcher_;
  int fetcher_id_;
  bool in_startup_sleep_;  // True if we're in the five-second "no fetching"
                           // period that begins at browser start.
  bool already_fetched_;   // True if we've already fetched a URL once this run;
                           // we won't fetch again until after a restart.
  bool need_to_fetch_;     // True if a consumer actually wants us to fetch an
                           // updated URL.  If this is never set, we won't
                           // bother to fetch anything.
                           // Consumers should observe
                           // chrome::NOTIFICATION_GOOGLE_URL_UPDATED.
  bool need_to_prompt_;    // True if the last fetched Google URL is not
                           // matched with current user's default Google URL
                           // nor the last prompted Google URL.
  bool search_committed_;  // True when we're expecting a notification of a new
                           // pending search navigation.
  EntryMap entry_map_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTracker);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
