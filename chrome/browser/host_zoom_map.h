// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom zoom levels.  Written on the UI thread and read on
// any thread.  One instance per profile.

#ifndef CHROME_BROWSER_HOST_ZOOM_MAP_H_
#define CHROME_BROWSER_HOST_ZOOM_MAP_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class PrefService;
class Profile;

class HostZoomMap : public NotificationObserver,
                    public base::RefCountedThreadSafe<HostZoomMap> {
 public:
  explicit HostZoomMap(Profile* profile);

  static void RegisterUserPrefs(PrefService* prefs);

  // Returns the zoom level for a given url. The zoom level is determined by
  // the host portion of the URL, or (in the absence of a host) the complete
  // spec of the URL. In most cases, there is no custom zoom level, and this
  // returns 0.  Otherwise, returns the saved zoom level, which may be positive
  // (to zoom in) or negative (to zoom out).
  //
  // This may be called on any thread.
  int GetZoomLevel(const GURL& url) const;

  // Sets the zoom level for a given url to |level|.  If the level is 0,
  // the host is erased from the saved preferences; otherwise the new value is
  // written out.
  //
  // This should only be called on the UI thread.
  void SetZoomLevel(const GURL& url, int level);

  // Resets all zoom levels.
  //
  // This should only be called on the UI thread.
  void ResetToDefaults();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class base::RefCountedThreadSafe<HostZoomMap>;

  typedef std::map<std::string, int> HostZoomLevels;

  ~HostZoomMap();

  // Reads the zoom levels from the preferences service.
  void Load();

  // Removes dependencies on the profile so we can live longer than
  // the profile without crashing.
  void Shutdown();

  // The profile we're associated with.
  Profile* profile_;

  // Copy of the pref data, so that we can read it on the IO thread.
  HostZoomLevels host_zoom_levels_;

  // Used around accesses to |host_zoom_levels_| to guarantee thread safety.
  mutable Lock lock_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preference service that we triggered ourself.
  bool updating_preferences_;

  NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(HostZoomMap);
};

#endif  // CHROME_BROWSER_HOST_ZOOM_MAP_H_
