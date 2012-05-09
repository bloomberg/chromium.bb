// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
#pragma once

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "net/base/network_change_notifier.h"

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
// the notification service for NOTIFY_GOOGLE_URL_UPDATED, and call GoogleURL()
// again after receiving it, in order to get the updated value.
//
// To protect users' privacy and reduce server load, no updates will be
// performed (ever) unless at least one consumer registers interest by calling
// RequestServerCheck().
class GoogleURLTracker : public content::URLFetcherDelegate,
                         public content::NotificationObserver,
                         public net::NetworkChangeNotifier::IPAddressObserver,
                         public ProfileKeyedService {
 public:
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

  typedef std::map<const InfoBarTabHelper*,
                   GoogleURLTrackerInfoBarDelegate*> InfoBarMap;
  typedef GoogleURLTrackerInfoBarDelegate* (*InfoBarCreator)(
      InfoBarTabHelper* infobar_helper,
      const GURL& search_url,
      GoogleURLTracker* google_url_tracker,
      const GURL& new_google_url);

  void AcceptGoogleURL(const GURL& google_url);
  void CancelGoogleURL(const GURL& google_url);
  void InfoBarClosed(const InfoBarTabHelper* infobar_helper);

  // Registers consumer interest in getting an updated URL from the server.
  // It will be notified as chrome::GOOGLE_URL_UPDATED, so the
  // consumer should observe this notification before calling this.
  void SetNeedToFetch();

  // Called when the five second startup sleep has finished.  Runs any pending
  // fetch.
  void FinishSleep();

  // Starts the fetch of the up-to-date Google URL if we actually want to fetch
  // it and can currently do so.
  void StartFetchIfDesirable();

  // content::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  // ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Called each time the user performs a search.  This checks whether we need
  // to prompt the user about a domain change, and if so, starts listening for
  // the notifications sent when the actual load is triggered.
  void SearchCommitted();

  // Called by Observe() after SearchCommitted() registers notification
  // listeners, to indicate that we've received the "load now pending"
  // notification.  |navigation_controller_source| and |web_contents_source| are
  // NotificationSources pointing to the associated NavigationController and
  // WebContents, respectively, for this load; |infobar_helper| is the
  // InfoBarTabHelper of the associated tab; and |search_url| is the actual
  // search performed by the user, which if necessary we'll re-do on a new
  // domain later.  This function creates a (still-invisible) InfoBarDelegate
  // for the associated tab and begins listening for the "load committed"
  // notification that will tell us it's safe to show the infobar.
  void OnNavigationPending(
      const content::NotificationSource& navigation_controller_source,
      const content::NotificationSource& web_contents_source,
      InfoBarTabHelper* infobar_helper,
      const GURL& search_url);

  // Called by Observe() once a load we're watching commits, or the associated
  // tab is closed.  The first three args are the same as for
  // OnNavigationPending(); |navigated| is true when this call is due to a
  // successful navigation (indicating that we should show our infobar) as
  // opposed to tab closue (which means we should delete the infobar).
  void OnNavigationCommittedOrTabClosed(
      const content::NotificationSource& navigation_controller_source,
      const content::NotificationSource& web_contents_source,
      const InfoBarTabHelper* infobar_helper,
      bool navigated);

  // Closes all open infobars.  If |redo_searches| is true, this also triggers
  // each tab to re-perform the user's search, but on the new Google TLD.
  void CloseAllInfoBars(bool redo_searches);

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  InfoBarCreator infobar_creator_;
  // TODO(ukai): GoogleURLTracker should track google domain (e.g. google.co.uk)
  // rather than URL (e.g. http://www.google.co.uk/), so that user could
  // configure to use https in search engine templates.
  GURL google_url_;
  GURL fetched_google_url_;
  base::WeakPtrFactory<GoogleURLTracker> weak_ptr_factory_;
  scoped_ptr<content::URLFetcher> fetcher_;
  int fetcher_id_;
  bool in_startup_sleep_;  // True if we're in the five-second "no fetching"
                           // period that begins at browser start.
  bool already_fetched_;   // True if we've already fetched a URL once this run;
                           // we won't fetch again until after a restart.
  bool need_to_fetch_;     // True if a consumer actually wants us to fetch an
                           // updated URL.  If this is never set, we won't
                           // bother to fetch anything.
                           // Consumers should observe
                           // chrome::GOOGLE_URL_UPDATED.
  bool need_to_prompt_;    // True if the last fetched Google URL is not
                           // matched with current user's default Google URL
                           // nor the last prompted Google URL.
  InfoBarMap infobar_map_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTracker);
};


// This infobar delegate is declared here (rather than in the .cc file) so test
// code can subclass it.
class GoogleURLTrackerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  GoogleURLTrackerInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                  const GURL& search_url,
                                  GoogleURLTracker* google_url_tracker,
                                  const GURL& new_google_url);

  // ConfirmInfoBarDelegate:
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // These are virtual so test code can override them in a subclass.
  virtual void Show();
  virtual void Close(bool redo_search);

 protected:
  virtual ~GoogleURLTrackerInfoBarDelegate();

  InfoBarTabHelper* map_key_;  // What |google_url_tracker_| uses to track us.
  const GURL search_url_;
  GoogleURLTracker* google_url_tracker_;
  const GURL new_google_url_;
  bool showing_;  // True if this delegate has been added to a TabContents.

 private:
  // ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;

  // Returns the portion of the appropriate hostname to display.
  string16 GetHost(bool new_host) const;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
