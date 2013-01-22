// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/window_container_type.h"
#include "googleurl/src/gurl.h"
#include "ui/base/window_open_disposition.h"

class CommandLine;
class PrefService;
class Profile;

namespace base {
class DictionaryValue;
}

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
class BackgroundContentsService : private content::NotificationObserver,
                                  public BackgroundContents::Delegate,
                                  public ProfileKeyedService {
 public:
  BackgroundContentsService(Profile* profile, const CommandLine* command_line);
  virtual ~BackgroundContentsService();

  // Returns the BackgroundContents associated with the passed application id,
  // or NULL if none.
  BackgroundContents* GetAppBackgroundContents(const string16& appid);

  // Returns true if there's a registered BackgroundContents for this app. It
  // is possible for this routine to return true when GetAppBackgroundContents()
  // returns false, if the BackgroundContents closed due to the render process
  // crashing.
  bool HasRegisteredBackgroundContents(const string16& appid);

  // Returns all currently opened BackgroundContents (used by the task manager).
  std::vector<BackgroundContents*> GetBackgroundContents() const;

  // BackgroundContents::Delegate implementation.
  virtual void AddWebContents(content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;

  // Gets the parent application id for the passed BackgroundContents. Returns
  // an empty string if no parent application found (e.g. passed
  // BackgroundContents has already shut down).
  const string16& GetParentApplicationId(BackgroundContents* contents) const;

  // Creates a new BackgroundContents using the passed |site| and
  // the |route_id| and begins tracking the object internally so it can be
  // shutdown if the parent application is uninstalled.
  // A BACKGROUND_CONTENTS_OPENED notification will be generated with the passed
  // |frame_name| and |application_id| values, using the passed |profile| as the
  // Source..
  BackgroundContents* CreateBackgroundContents(content::SiteInstance* site,
                                               int route_id,
                                               Profile* profile,
                                               const string16& frame_name,
                                               const string16& application_id);

  // Load the manifest-specified background page for the specified hosted app.
  // If the manifest doesn't specify one, then load the BackgroundContents
  // registered in the pref. This is typically used to reload a crashed
  // background page.
  void LoadBackgroundContentsForExtension(Profile* profile,
                                          const std::string& extension_id);

 private:
  friend class BackgroundContentsServiceTest;
  friend class MockBackgroundContents;
  friend class TaskManagerBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(BackgroundContentsServiceTest,
                           BackgroundContentsCreateDestroy);
  FRIEND_TEST_ALL_PREFIXES(BackgroundContentsServiceTest,
                           TestApplicationIDLinkage);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerBrowserTest,
                           NoticeBGContentsChanges);
  FRIEND_TEST_ALL_PREFIXES(TaskManagerBrowserTest,
                           KillBGContents);

  // Registers for various notifications.
  void StartObserving(Profile* profile);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Loads all registered BackgroundContents at startup.
  void LoadBackgroundContentsFromPrefs(Profile* profile);

  // Load a BackgroundContent; the settings are read from the provided
  // dictionary.
  void LoadBackgroundContentsFromDictionary(
      Profile* profile,
      const std::string& extension_id,
      const base::DictionaryValue* contents);

  // Load the manifest-specified BackgroundContents for all apps for the
  // profile.
  void LoadBackgroundContentsFromManifests(Profile* profile);

  // Creates a single BackgroundContents associated with the specified |appid|,
  // creates an associated RenderView with the name specified by |frame_name|,
  // and navigates to the passed |url|.
  void LoadBackgroundContents(Profile* profile,
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

  // Sends out a notification when our association of background contents with
  // apps may have changed (used by BackgroundApplicationListModel to update the
  // set of background apps as new background contents are opened/closed).
  void SendChangeNotification(Profile* profile);

  // PrefService used to store list of background pages (or NULL if this is
  // running under an incognito profile).
  PrefService* prefs_;
  content::NotificationRegistrar registrar_;

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

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_H_
