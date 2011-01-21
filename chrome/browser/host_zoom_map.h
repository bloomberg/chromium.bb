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
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class PrefService;
class Profile;

// HostZoomMap needs to be deleted on the UI thread because it listens
// to notifications on there (and holds a NotificationRegistrar).
class HostZoomMap :
    public NotificationObserver,
    public base::RefCountedThreadSafe<HostZoomMap,
                                      BrowserThread::DeleteOnUIThread> {
 public:
  explicit HostZoomMap(Profile* profile);

  static void RegisterUserPrefs(PrefService* prefs);

  // Returns the zoom level for a given url. The zoom level is determined by
  // the host portion of the URL, or (in the absence of a host) the complete
  // spec of the URL. In most cases, there is no custom zoom level, and this
  // returns the user's default zoom level.  Otherwise, returns the saved zoom
  // level, which may be positive (to zoom in) or negative (to zoom out).
  //
  // This may be called on any thread.
  double GetZoomLevel(const GURL& url) const;

  // Sets the zoom level for a given url to |level|.  If the level matches the
  // current default zoom level, the host is erased from the saved preferences;
  // otherwise the new value is written out.
  //
  // This should only be called on the UI thread.
  void SetZoomLevel(const GURL& url, double level);

  // Returns the temporary zoom level that's only valid for the lifetime of
  // the given tab (i.e. isn't saved and doesn't affect other tabs) if it
  // exists, the default zoom level otherwise.
  //
  // This may be called on any thread.
  double GetTemporaryZoomLevel(int render_process_id,
                               int render_view_id) const;

  // Sets the temporary zoom level that's only valid for the lifetime of this
  // tab.
  //
  // This should only be called on the UI thread.
  void SetTemporaryZoomLevel(int render_process_id,
                             int render_view_id,
                             double level);

  // Resets all zoom levels.
  //
  // This should only be called on the UI thread.
  void ResetToDefaults();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<HostZoomMap>;

  typedef std::map<std::string, double> HostZoomLevels;

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
  double default_zoom_level_;

  struct TemporaryZoomLevel {
    int render_process_id;
    int render_view_id;
    double zoom_level;
  };

  // Don't expect more than a couple of tabs that are using a temporary zoom
  // level, so vector is fine for now.
  std::vector<TemporaryZoomLevel> temporary_zoom_levels_;

  // Used around accesses to |host_zoom_levels_|, |default_zoom_level_| and
  // |temporary_zoom_levels_| to guarantee thread safety.
  mutable base::Lock lock_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preference service that we triggered ourself.
  bool updating_preferences_;

  NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(HostZoomMap);
};

#endif  // CHROME_BROWSER_HOST_ZOOM_MAP_H_
