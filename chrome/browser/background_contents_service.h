// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_CONTENTS_SERVICE_H_
#define CHROME_BROWSER_BACKGROUND_CONTENTS_SERVICE_H_
#pragma once

#include <map>

#include "base/gtest_prod_util.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/window_container_type.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/window_open_disposition.h"

class BackgroundContents;
class CommandLine;
class PrefService;
class Profile;
class TabContents;

namespace gfx {
class Rect;
}

struct BackgroundContentsOpenedDetails;

// BackgroundContentsService is owned by the profile, and is responsible for
// managing the lifetime of BackgroundContents (tracking the set of background
// urls, loading them at startup, and keeping the browser process alive as long
// as there are BackgroundContents loaded).
//
// It is also responsible for tracking the association between
// BackgroundContents and their parent app, and shutting them down when the
// parent app is unloaded.
class BackgroundContentsService : private NotificationObserver,
                                  public BackgroundContents::Delegate {
 public:
  BackgroundContentsService(Profile* profile, const CommandLine* command_line);
  virtual ~BackgroundContentsService();

  // Returns the BackgroundContents associated with the passed application id,
  // or NULL if none.
  BackgroundContents* GetAppBackgroundContents(const string16& appid);

  static void RegisterUserPrefs(PrefService* prefs);

  // BackgroundContents::Delegate implementation.
  virtual void AddTabContents(TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);

 private:
  friend class BackgroundContentsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(BackgroundContentsServiceTest,
                           BackgroundContentsCreateDestroy);
  FRIEND_TEST_ALL_PREFIXES(BackgroundContentsServiceTest,
                           TestApplicationIDLinkage);

  // Registers for various notifications.
  void StartObserving(Profile* profile);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Loads all registered BackgroundContents at startup.
  void LoadBackgroundContentsFromPrefs(Profile* profile);

  // Creates a single BackgroundContents associated with the specified |appid|.
  // The BackgroundContents frame will be given the name specified by
  // |frame_name| and navigated to the passed URL.
  void CreateBackgroundContents(Profile* profile,
                                const GURL& url,
                                const string16& frame_name,
                                const string16& appid);

  // Invoked when a new BackgroundContents is opened.
  void BackgroundContentsOpened(BackgroundContentsOpenedDetails* details);

  // Invoked when a BackgroundContents object is destroyed.
  void BackgroundContentsShutdown(BackgroundContents* contents);

  // Registers the |contents->GetURL()| to be run at startup. Only happens for
  // the first navigation after window.open() (future calls to
  // RegisterBackgroundContents() for the same BackgroundContents will do
  // nothing).
  void RegisterBackgroundContents(BackgroundContents* contents);

  // Stops loading the passed BackgroundContents on startup.
  void UnregisterBackgroundContents(BackgroundContents* contents);

  // Unregisters and deletes the BackgroundContents associated with the
  // passed extension.
  void ShutdownAssociatedBackgroundContents(const string16& appid);

  // Returns true if this BackgroundContents is in the contents_list_.
  bool IsTracked(BackgroundContents* contents) const;

  // Gets the parent application id for the passed BackgroundContents. Returns
  // an empty string if no parent application found (e.g. passed
  // BackgroundContents has already shut down).
  const string16& GetParentApplicationId(BackgroundContents* contents) const;

  // PrefService used to store list of background pages (or NULL if this is
  // running under an off-the-record profile).
  PrefService* prefs_;
  NotificationRegistrar registrar_;

  // Information we track about each BackgroundContents.
  struct BackgroundContentsInfo {
    // The BackgroundContents whose information we are tracking.
    BackgroundContents* contents;
    // The name of the top level frame for this BackgroundContents.
    string16 frame_name;
  };

  // Map associating currently loaded BackgroundContents with their parent
  // applications.
  // Key: application id
  // Value: BackgroundContentsInfo for the BC associated with that application
  typedef std::map<string16, BackgroundContentsInfo> BackgroundContentsMap;
  BackgroundContentsMap contents_map_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsService);
};

#endif  // CHROME_BROWSER_BACKGROUND_CONTENTS_SERVICE_H_
