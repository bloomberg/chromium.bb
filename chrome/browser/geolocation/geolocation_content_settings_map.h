// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps [requesting_origin, embedder] to content settings.  Written on the UI
// thread and read on any thread.  One instance per profile. This is based on
// HostContentSettingsMap but differs significantly in two aspects:
// - It maps [requesting_origin.GetOrigin(), embedder.GetOrigin()] => setting
//   rather than host => setting.
// - It manages only Geolocation.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONTENT_SETTINGS_MAP_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONTENT_SETTINGS_MAP_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/content_settings.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class ContentSettingsDetails;
class DictionaryValue;
class PrefService;
class Profile;

class GeolocationContentSettingsMap
    : public base::RefCountedThreadSafe<GeolocationContentSettingsMap>,
      public NotificationObserver {
 public:
  typedef std::map<GURL, ContentSetting> OneOriginSettings;
  typedef std::map<GURL, OneOriginSettings> AllOriginsSettings;

  explicit GeolocationContentSettingsMap(Profile* profile);

  virtual ~GeolocationContentSettingsMap();

  static void RegisterUserPrefs(PrefService* prefs);

  // Returns the default setting.
  //
  // This should only be called on the UI thread.
  ContentSetting GetDefaultContentSetting() const;

  // Returns true if the content setting is managed (set by a policy).
  bool IsDefaultContentSettingManaged() const;

  // Returns a single ContentSetting which applies to the given |requesting_url|
  // when embedded in a top-level page from |embedding_url|.  To determine the
  // setting for a top-level page, as opposed to a frame embedded in a page,
  // pass the page's URL for both arguments.
  //
  // This should only be called on the UI thread.
  // Both arguments should be valid GURLs.
  ContentSetting GetContentSetting(const GURL& requesting_url,
                                   const GURL& embedding_url) const;

  // Returns the settings for all origins with any non-default settings.
  //
  // This should only be called on the UI thread.
  AllOriginsSettings GetAllOriginsSettings() const;

  // Sets the default setting.
  //
  // This should only be called on the UI thread.
  void SetDefaultContentSetting(ContentSetting setting);

  // Sets the content setting for a particular (requesting origin, embedding
  // origin) pair.  If the embedding origin is the same as the requesting
  // origin, this represents the setting used when the requesting origin is
  // itself the top-level page.  If |embedder| is the empty GURL, |setting|
  // becomes the default setting for the requesting origin when embedded on any
  // page that does not have an explicit setting.  Passing
  // CONTENT_SETTING_DEFAULT for |setting| effectively removes that setting and
  // allows future requests to return the all-embedders or global defaults (as
  // applicable).
  //
  // This should only be called on the UI thread.  |requesting_url| should be
  // a valid GURL, and |embedding_url| should be valid or empty.
  void SetContentSetting(const GURL& requesting_url,
                         const GURL& embedding_url,
                         ContentSetting setting);

  // Resets all settings.
  //
  // This should only be called on the UI thread.
  void ResetToDefault();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class base::RefCountedThreadSafe<GeolocationContentSettingsMap>;

  // The default setting.
  static const ContentSetting kDefaultSetting;

  // Sends a CONTENT_SETTINGS_CHANGED notification.
  void NotifyObservers(const ContentSettingsDetails& details);

  void UnregisterObservers();

  // Sets the fields of |one_origin_settings| based on the values in
  // |dictionary|.
  static void GetOneOriginSettingsFromDictionary(
      const DictionaryValue* dictionary,
      OneOriginSettings* one_origin_settings);

  // The profile we're associated with.
  Profile* profile_;

  // Registrar to register for PREF_CHANGED notifications.
  PrefChangeRegistrar prefs_registrar_;
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationContentSettingsMap);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONTENT_SETTINGS_MAP_H_
