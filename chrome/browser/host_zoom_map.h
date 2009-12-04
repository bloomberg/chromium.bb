// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom zoom levels.  Written on the UI thread and read on
// the IO thread.  One instance per profile.

#ifndef CHROME_BROWSER_HOST_ZOOM_MAP_H_
#define CHROME_BROWSER_HOST_ZOOM_MAP_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/ref_counted.h"

class PrefService;
class Profile;

class HostZoomMap : public base::RefCountedThreadSafe<HostZoomMap> {
 public:
  explicit HostZoomMap(Profile* profile);

  static void RegisterUserPrefs(PrefService* prefs);

  // Returns the zoom level for a given hostname.  In most cases, there is no
  // custom zoom level, and this returns 0.  Otherwise, returns the saved zoom
  // level, which may be positive (to zoom in) or negative (to zoom out).
  //
  // This may be called on any thread.
  int GetZoomLevel(const std::string& host) const;

  // Sets the zoom level for a given hostname to |level|.  If the level is 0,
  // the host is erased from the saved preferences; otherwise the new value is
  // written out.
  //
  // This should only be called on the UI thread.
  void SetZoomLevel(const std::string& host, int level);

 private:
  friend class base::RefCountedThreadSafe<HostZoomMap>;

  typedef std::map<std::string, int> HostZoomLevels;

  ~HostZoomMap();

  // The profile we're associated with.
  Profile* profile_;

  // Copy of the pref data, so that we can read it on the IO thread.
  HostZoomLevels host_zoom_levels_;

  // Used around accesses to |host_zoom_levels_| to guarantee thread safety.
  mutable Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(HostZoomMap);
};

#endif  // CHROME_BROWSER_HOST_ZOOM_MAP_H_
