// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_PAGE_TRACKER_H_
#define CHROME_BROWSER_BACKGROUND_PAGE_TRACKER_H_
#pragma once

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/singleton.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class PrefService;

///////////////////////////////////////////////////////////////////////////////
// BackgroundPageTracker
//
// This class is a singleton class that monitors when new background pages are
// loaded, and sends out a notification.
//
class BackgroundPageTracker : public NotificationObserver {
 public:
  static void RegisterPrefs(PrefService* prefs);

  // Convenience routine which gets the singleton object.
  static BackgroundPageTracker* GetInstance();

  // Returns the number of background apps/extensions currently loaded.
  int GetBackgroundPageCount();

  // Returns the number of unacknowledged background pages (for use in badges,
  // etc).
  int GetUnacknowledgedBackgroundPageCount();

  // Called when the user has acknowledged that new background apps have been
  // loaded. Future calls to GetUnacknowledgedBackgroundPageCount() will return
  // 0 until another background page is installed.
  void AcknowledgeBackgroundPages();

 protected:
  // Constructor marked as protected so we can create mock subclasses.
  BackgroundPageTracker();
  ~BackgroundPageTracker();
  friend struct DefaultSingletonTraits<BackgroundPageTracker>;
  friend class BackgroundPageTrackerTest;
  FRIEND_TEST_ALL_PREFIXES(BackgroundPageTrackerTest, OnBackgroundPageLoaded);
  FRIEND_TEST_ALL_PREFIXES(BackgroundPageTrackerTest,
                           AcknowledgeBackgroundPages);
  FRIEND_TEST_ALL_PREFIXES(BackgroundPageTrackerTest,
                           TestTrackerChangedNotifications);

  // Mockable method to get the PrefService where we store information about
  // background pages.
  virtual PrefService* GetPrefService();

  // Helper routine which checks to see if this object should be enabled or not.
  // If false, the BackgroundPageTracker always acts as if there are no
  // background pages. Virtual to allow enabling in mocks.
  virtual bool IsEnabled();

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Invoked once all extensions have been loaded, so we can update our list of
  // tracked pages and send out the appropriate notifications. Returns true if
  // the extension list was changed.
  bool UpdateExtensionList();

  // Called when a background page is loaded (either because a hosted app
  // opened a BackgroundContents or a packaged extension was loaded with an
  // extension background page). Updates our internal list of unacknowledged
  // background pages.
  void OnBackgroundPageLoaded(const std::string& parent_application_id);

  // When an extension is unloaded, removes it from the list of background pages
  // so we no longer nag the user about it (if it is still unacknowledged) and
  // we inform the user if it is ever re-installed/enabled.
  void OnExtensionUnloaded(const std::string& parent_application_id);

  // Sends out a notification that the list of background pages has changed.
  void SendChangeNotification();

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundPageTracker);
};

#endif  // CHROME_BROWSER_BACKGROUND_PAGE_TRACKER_H_
