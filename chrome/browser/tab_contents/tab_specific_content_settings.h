// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/geolocation/geolocation_settings_state.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/tab_contents/navigation_controller.h"

class CannedBrowsingDataAppCacheHelper;
class CannedBrowsingDataDatabaseHelper;
class CannedBrowsingDataIndexedDBHelper;
class CannedBrowsingDataLocalStorageHelper;
class CookiesTreeModel;
class Profile;

namespace net {
class CookieMonster;
}

class TabSpecificContentSettings
    : public RenderViewHostDelegate::ContentSettings {
 public:
  class Delegate {
   public:
    // Invoked when content settings for resources in the tab contents
    // associated with this TabSpecificContentSettings object were accessed.
    // |content_was_blocked| is true, if a content settings type was blocked
    // (as opposed to just accessed). Currently, this parameter is checked in
    // unit tests only.
    virtual void OnContentSettingsAccessed(bool content_was_blocked) = 0;

    virtual ~Delegate() {}
  };

  TabSpecificContentSettings(Delegate* delegate, Profile* profile);

  virtual ~TabSpecificContentSettings() {}

  // Resets the |content_blocked_| and |content_accessed_| arrays, except for
  // CONTENT_SETTINGS_TYPE_COOKIES related information.
  void ClearBlockedContentSettingsExceptForCookies();

  // Resets all cookies related information.
  void ClearCookieSpecificContentSettings();

  // Clears the Geolocation settings.
  void ClearGeolocationContentSettings();

  // Changes the |content_blocked_| entry for popups.
  void SetPopupsBlocked(bool blocked);

  // Updates Geolocation settings on navigation.
  void GeolocationDidNavigate(
      const NavigationController::LoadCommittedDetails& details);

  // Returns whether a particular kind of content has been blocked for this
  // page.
  bool IsContentBlocked(ContentSettingsType content_type) const;

  // Returns true if content blockage was indicated to the user.
  bool IsBlockageIndicated(ContentSettingsType content_type) const;

  void SetBlockageHasBeenIndicated(ContentSettingsType content_type);

  // Returns whether a particular kind of content has been accessed. Currently
  // only tracks cookies.
  bool IsContentAccessed(ContentSettingsType content_type) const;

  const std::set<std::string>& BlockedResourcesForType(
      ContentSettingsType content_type) const;

  // Returns the GeolocationSettingsState that controls the
  // geolocation API usage on this page.
  const GeolocationSettingsState& geolocation_settings_state() const {
    return geolocation_settings_state_;
  }

  // Returns a CookiesTreeModel object for the recoreded allowed cookies.
  CookiesTreeModel* GetAllowedCookiesTreeModel();

  // Returns a CookiesTreeModel object for the recoreded blocked cookies.
  CookiesTreeModel* GetBlockedCookiesTreeModel();

  bool load_plugins_link_enabled() { return load_plugins_link_enabled_; }
  void set_load_plugins_link_enabled(bool enabled) {
    load_plugins_link_enabled_ = enabled;
  }

  // RenderViewHostDelegate::ContentSettings implementation.
  virtual void OnContentBlocked(ContentSettingsType type,
                                const std::string& resource_identifier);
  virtual void OnCookiesRead(const GURL& url,
                             const net::CookieList& cookie_list,
                             bool blocked_by_policy);
  virtual void OnCookieChanged(const GURL& url,
                               const std::string& cookie_line,
                               const net::CookieOptions& options,
                               bool blocked_by_policy);
  virtual void OnIndexedDBAccessed(const GURL& url,
                                   const string16& description,
                                   bool blocked_by_policy);
  virtual void OnLocalStorageAccessed(const GURL& url,
                                      DOMStorageType storage_type,
                                      bool blocked_by_policy);
  virtual void OnWebDatabaseAccessed(const GURL& url,
                                     const string16& name,
                                     const string16& display_name,
                                     unsigned long estimated_size,
                                     bool blocked_by_policy);
  virtual void OnAppCacheAccessed(const GURL& manifest_url,
                                  bool blocked_by_policy);
  virtual void OnGeolocationPermissionSet(const GURL& requesting_frame,
                                          bool allowed);

 private:
  class LocalSharedObjectsContainer {
   public:
    explicit LocalSharedObjectsContainer(Profile* profile);
    ~LocalSharedObjectsContainer();

    // Empties the container.
    void Reset();

    net::CookieMonster* cookies() const { return cookies_; }
    CannedBrowsingDataAppCacheHelper* appcaches() const {
      return appcaches_;
    }
    CannedBrowsingDataDatabaseHelper* databases() const {
      return databases_;
    }
    CannedBrowsingDataIndexedDBHelper* indexed_dbs() const {
      return indexed_dbs_;
    }
    CannedBrowsingDataLocalStorageHelper* local_storages() const {
      return local_storages_;
    }
    CannedBrowsingDataLocalStorageHelper* session_storages() const {
      return session_storages_;
    }

    CookiesTreeModel* GetCookiesTreeModel();

    bool empty() const;

   private:
    DISALLOW_COPY_AND_ASSIGN(LocalSharedObjectsContainer);

    scoped_refptr<net::CookieMonster> cookies_;
    scoped_refptr<CannedBrowsingDataAppCacheHelper> appcaches_;
    scoped_refptr<CannedBrowsingDataDatabaseHelper> databases_;
    scoped_refptr<CannedBrowsingDataIndexedDBHelper> indexed_dbs_;
    scoped_refptr<CannedBrowsingDataLocalStorageHelper> local_storages_;
    scoped_refptr<CannedBrowsingDataLocalStorageHelper> session_storages_;
  };

  void AddBlockedResource(ContentSettingsType content_type,
                          const std::string& resource_identifier);

  void OnContentAccessed(ContentSettingsType type);

  // Stores which content setting types actually have blocked content.
  bool content_blocked_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores if the blocked content was messaged to the user.
  bool content_blockage_indicated_to_user_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores which content setting types actually were accessed.
  bool content_accessed_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores the blocked resources for each content type.
  // Currently only used for plugins.
  scoped_ptr<std::set<std::string> >
      blocked_resources_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores the blocked/allowed cookies.
  LocalSharedObjectsContainer allowed_local_shared_objects_;
  LocalSharedObjectsContainer blocked_local_shared_objects_;

  // Manages information about Geolocation API usage in this page.
  GeolocationSettingsState geolocation_settings_state_;

  // Stores whether the user can load blocked plugins on this page.
  bool load_plugins_link_enabled_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabSpecificContentSettings);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
