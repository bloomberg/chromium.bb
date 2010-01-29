// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom zoom levels.  Written on the UI thread and read on
// the IO thread.  One instance per profile.

#ifndef CHROME_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
#define CHROME_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/lock.h"
#include "chrome/common/content_permission_types.h"

class PrefService;
class Profile;

class HostContentSettingsMap {
 public:
  typedef std::map<std::string, ContentPermissionType> HostContentPermissions;

  explicit HostContentSettingsMap(Profile* profile);

  static void RegisterUserPrefs(PrefService* prefs);

  void ResetToDefaults();

  // Returns a map of all hostnames with per host content settings to
  // their respective settings where a given |content_type| differs
  // from CONTENT_PERMISSION_TYPE_DEFAULT
  //
  // This may be called on any thread.
  HostContentPermissions GetAllPerHostContentPermissions(
      ContentSettingsType content_type) const;

  // Returns the default ContentPermissions for a specific ContentSettingsType.
  //
  // This may be called on any thread.
  ContentPermissionType GetDefaultContentPermission(
      ContentSettingsType type) const {
    return default_content_settings_.permissions[type];
  }

  // Returns the ContentPermissions for a specific ContentSettingsType.
  //
  // This may be called on any thread.
  ContentPermissionType GetPerHostContentPermission(const std::string& host,
      ContentSettingsType type) const {
    return GetPerHostContentSettings(host).permissions[type];
  }

  // Returns the ContentPermissions which apply to a given host.
  //
  // This may be called on any thread.
  ContentPermissions GetPerHostContentSettings(const std::string& host) const;

  // Sets the default ContentPermissions. Returns true on success.
  //
  // This should only be called on the UI thread.
  bool SetDefaultContentPermission(ContentSettingsType type,
                                   ContentPermissionType permission);

  // Sets per host ContentPermissions for a given host and CotentSettings. To
  // remove an exception for the host, set the permissions to
  // CONTENT_PERMISSIONS_TYPE_DEFAULT.
  //
  // This should only be called on the UI thread.
  void SetPerHostContentPermission(const std::string& host,
                                   ContentSettingsType type,
                                   ContentPermissionType permission);

  // Sets per host ContentPermissions for a given host.
  //
  // This should only be called on the UI thread.
  void SetPerHostContentSettings(const std::string& host,
                                 const ContentPermissions& permissions);

 private:
  typedef std::map<std::string, ContentPermissions> HostContentSettings;

  // The profile we're associated with.
  Profile* profile_;

  // Copy of the pref data, so that we can read it on the IO thread.
  HostContentSettings host_content_settings_;
  ContentPermissions default_content_settings_;

  // Used around accesses to |host_content_settings_| to guarantee thread
  // safety.
  mutable Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(HostContentSettingsMap);
};

#endif  // CHROME_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
