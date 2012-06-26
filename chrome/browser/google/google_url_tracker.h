// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
#pragma once

#include <map>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

class GoogleURLTrackerInfoBarDelegate;
class PrefService;
class Profile;

namespace content {
class NavigationController;
class WebContents;
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
                         public content::NotificationObserver,
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
  GoogleURLTracker(Profile* profile, Mode mode);

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
  // If prompting is necessary, we then listen for the subsequent
  // NAV_ENTRY_PENDING notification to get the appropriate NavigationController.
  // When the load commits, we'll show the infobar.
  //
  // When |profile| is NULL or a testing profile, this function does nothing.
  static void GoogleURLSearchCommitted(Profile* profile);

  static const char kDefaultGoogleHomepage[];
  static const char kSearchDomainCheckURL[];

 private:
  friend class GoogleURLTrackerInfoBarDelegate;
  friend class GoogleURLTrackerTest;

  struct MapEntry {
    MapEntry();  // Required by STL.
    MapEntry(GoogleURLTrackerInfoBarDelegate* infobar,
             const content::NotificationSource& navigation_controller_source,
             const content::NotificationSource& tab_contents_source);
    ~MapEntry();

    GoogleURLTrackerInfoBarDelegate* infobar;
    content::NotificationSource navigation_controller_source;
    content::NotificationSource tab_contents_source;
  };

  typedef std::map<const InfoBarTabHelper*, MapEntry> InfoBarMap;
  typedef GoogleURLTrackerInfoBarDelegate* (*InfoBarCreator)(
      InfoBarTabHelper* infobar_helper,
      GoogleURLTracker* google_url_tracker,
      const GURL& new_google_url);

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  // ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Callbacks from GoogleURLTrackerInfoBarDelegate:
  void AcceptGoogleURL(const GURL& google_url, bool redo_searches);
  void CancelGoogleURL(const GURL& google_url);
  void InfoBarClosed(const InfoBarTabHelper* infobar_helper);

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

  // Called by Observe() after SearchCommitted() registers notification
  // listeners, to indicate that we've received the "load now pending"
  // notification.  |navigation_controller_source| and |tab_contents_source| are
  // NotificationSources pointing to the associated NavigationController and
  // TabContents, respectively, for this load; |infobar_helper| is the
  // InfoBarTabHelper of the associated tab; and |search_url| is the actual
  // search performed by the user, which if necessary we'll re-do on a new
  // domain later.  If there is already a visible GoogleURLTracker infobar for
  // this tab, this function resets its associated navigation entry to point at
  // the new pending entry.  Otherwise this function creates a (still-invisible)
  // InfoBarDelegate for the associated tab.
  void OnNavigationPending(
      const content::NotificationSource& navigation_controller_source,
      const content::NotificationSource& tab_contents_source,
      InfoBarTabHelper* infobar_helper,
      int pending_id);

  // Called by Observe() once a load we're watching commits, or the associated
  // tab is closed.  |infobar_helper| is the same as for OnNavigationPending();
  // |search_url| is valid when this call is due to a successful navigation
  // (indicating that we should show or update the relevant infobar) as opposed
  // to tab closure (which means we should delete the infobar).
  void OnNavigationCommittedOrTabClosed(const InfoBarTabHelper* infobar_helper,
                                        const GURL& search_url);

  // Closes all open infobars.  If |redo_searches| is true, this also triggers
  // each tab to re-perform the user's search, but on the new Google TLD.
  void CloseAllInfoBars(bool redo_searches);

  // Unregisters any listeners for the notification sources in |map_entry|.
  // This also sanity-DCHECKs that these are registered (or not) in the specific
  // cases we expect.  (|must_be_listening_for_commit| is used purely for this
  // sanity-checking.)
  void UnregisterForEntrySpecificNotifications(
      const MapEntry& map_entry,
      bool must_be_listening_for_commit);

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  InfoBarCreator infobar_creator_;
  // TODO(ukai): GoogleURLTracker should track google domain (e.g. google.co.uk)
  // rather than URL (e.g. http://www.google.co.uk/), so that user could
  // configure to use https in search engine templates.
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
  InfoBarMap infobar_map_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTracker);
};


// This infobar delegate is declared here (rather than in the .cc file) so test
// code can subclass it.
class GoogleURLTrackerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  GoogleURLTrackerInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                  GoogleURLTracker* google_url_tracker,
                                  const GURL& new_google_url);

  // ConfirmInfoBarDelegate:
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;

  // Allows GoogleURLTracker to change the Google base URL after the infobar has
  // been instantiated.  This should only be called with an URL with the same
  // TLD as the existing one, so that the prompt we're displaying will still be
  // correct.
  void SetGoogleURL(const GURL& new_google_url);

  bool showing() const { return showing_; }
  void set_pending_id(int pending_id) { pending_id_ = pending_id; }

  // These are virtual so test code can override them in a subclass.
  virtual void Show(const GURL& search_url);
  virtual void Close(bool redo_search);

 protected:
  virtual ~GoogleURLTrackerInfoBarDelegate();

  InfoBarTabHelper* map_key_;  // What |google_url_tracker_| uses to track us.
  GURL search_url_;
  GoogleURLTracker* google_url_tracker_;
  GURL new_google_url_;
  bool showing_;  // True if this delegate has been added to a TabContents.
  int pending_id_;

 private:
  // ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
