// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/common/net/url_fetcher.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "net/base/network_change_notifier.h"

class NavigationController;
class PrefService;
class TabContents;
class TemplateURL;

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
class GoogleURLTracker : public URLFetcher::Delegate,
                         public NotificationObserver,
                         public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // Only the main browser process loop should call this, when setting up
  // g_browser_process->google_url_tracker_.  No code other than the
  // GoogleURLTracker itself should actually use
  // g_browser_process->google_url_tracker() (which shouldn't be hard, since
  // there aren't useful public functions on this object for consumers to access
  // anyway).
  GoogleURLTracker();

  virtual ~GoogleURLTracker();

  // Returns the current Google URL.  This will return a valid URL even in
  // unittest mode.
  //
  // This is the only function most code should ever call.
  static GURL GoogleURL();

  // Requests that the tracker perform a server check to update the Google URL
  // as necessary.  This will happen at most once per network change, not
  // sooner than five seconds after startup (checks requested before that time
  // will occur then; checks requested afterwards will occur immediately, if
  // no other checks have been made during this run).
  //
  // In unittest mode, this function does nothing.
  static void RequestServerCheck();

  static void RegisterPrefs(PrefService* prefs);

  // Notifies the tracker that the user has started a Google search.
  // If prompting is necessary, we then listen for the subsequent
  // NAV_ENTRY_PENDING notification to get the appropriate NavigationController.
  // When the load commits, we'll show the infobar.
  static void GoogleURLSearchCommitted();

  static const char kDefaultGoogleHomepage[];
  static const char kSearchDomainCheckURL[];

  // Methods called from InfoBar delegate.
  void AcceptGoogleURL(const GURL& google_url);
  void CancelGoogleURL(const GURL& google_url);
  void InfoBarClosed();
  void RedoSearch();

 private:
  friend class GoogleURLTrackerTest;

  typedef InfoBarDelegate* (*InfobarCreator)(TabContents*,
                                             GoogleURLTracker*,
                                             const GURL&);

  // Registers consumer interest in getting an updated URL from the server.
  // It will be notified as NotificationType::GOOGLE_URL_UPDATED, so the
  // consumer should observe this notification before calling this.
  void SetNeedToFetch();

  // Begins the five-second startup sleep period, unless a test has cleared
  // |queue_wakeup_task_|.
  void QueueWakeupTask();

  // Called when the five second startup sleep has finished.  Runs any pending
  // fetch.
  void FinishSleep();

  // Starts the fetch of the up-to-date Google URL if we actually want to fetch
  // it and can currently do so.
  void StartFetchIfDesirable();

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher *source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // NetworkChangeNotifier::IPAddressObserver
  virtual void OnIPAddressChanged();

  void SearchCommitted();
  void OnNavigationPending(const NotificationSource& source,
                           const GURL& pending_url);
  void OnNavigationCommittedOrTabClosed(TabContents* tab_contents,
                                        NotificationType::Type type);
  void ShowGoogleURLInfoBarIfNecessary(TabContents* tab_contents);

  NotificationRegistrar registrar_;
  InfobarCreator infobar_creator_;
  // TODO(ukai): GoogleURLTracker should track google domain (e.g. google.co.uk)
  // rather than URL (e.g. http://www.google.co.uk/), so that user could
  // configure to use https in search engine templates.
  GURL google_url_;
  GURL fetched_google_url_;
  ScopedRunnableMethodFactory<GoogleURLTracker> runnable_method_factory_;
  scoped_ptr<URLFetcher> fetcher_;
  int fetcher_id_;
  bool queue_wakeup_task_;
  bool in_startup_sleep_;  // True if we're in the five-second "no fetching"
                           // period that begins at browser start.
  bool already_fetched_;   // True if we've already fetched a URL once this run;
                           // we won't fetch again until after a restart.
  bool need_to_fetch_;     // True if a consumer actually wants us to fetch an
                           // updated URL.  If this is never set, we won't
                           // bother to fetch anything.
                           // Consumers should observe
                           // NotificationType::GOOGLE_URL_UPDATED.
  bool need_to_prompt_;    // True if the last fetched Google URL is not
                           // matched with current user's default Google URL
                           // nor the last prompted Google URL.
  NavigationController* controller_;
  InfoBarDelegate* infobar_;
  GURL search_url_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTracker);
};


// This infobar delegate is declared here (rather than in the .cc file) so test
// code can subclass it.
class GoogleURLTrackerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  GoogleURLTrackerInfoBarDelegate(TabContents* tab_contents,
                                  GoogleURLTracker* google_url_tracker,
                                  const GURL& new_google_url);

  // ConfirmInfoBarDelegate:
  virtual bool Accept();
  virtual bool Cancel();
  virtual void InfoBarClosed();

 protected:
  virtual ~GoogleURLTrackerInfoBarDelegate();

  GoogleURLTracker* google_url_tracker_;
  const GURL new_google_url_;

 private:
  // ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_H_
