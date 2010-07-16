// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_SPECIFIC_CONTENT_SETTINGS_H_

#include "base/basictypes.h"
#include "chrome/browser/geolocation/geolocation_settings_state.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"

class CannedBrowsingDataAppCacheHelper;
class CannedBrowsingDataDatabaseHelper;
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
    // Invoked when content settings managed by the TabSpecificContentSettings
    // object change.
    virtual void OnContentSettingsChange() = 0;

    virtual ~Delegate() {}
  };

  TabSpecificContentSettings(Delegate* delegate, Profile* profile);

  virtual ~TabSpecificContentSettings() {}

  // Resets the |content_blocked_| array and stored cookies.
  void ClearBlockedContentSettings();

  // Changes the |content_blocked_| entry for popups.
  void SetPopupsBlocked(bool blocked);

  // Updates Geolocation settings on navigation.
  void GeolocationDidNavigate(
      const NavigationController::LoadCommittedDetails& details);

  // Returns whether a particular kind of content has been blocked for this
  // page.
  bool IsContentBlocked(ContentSettingsType content_type) const;

  // Returns the GeolocationSettingsState that controls the
  // geolocation API usage on this page.
  const GeolocationSettingsState& geolocation_settings_state() const {
    return geolocation_settings_state_;
  }

  // Returns a CookiesTreeModel object for the recoreded allowed cookies.
  CookiesTreeModel* GetAllowedCookiesTreeModel();

  // Returns a CookiesTreeModel object for the recoreded blocked cookies.
  CookiesTreeModel* GetBlockedCookiesTreeModel();

  // RenderViewHostDelegate::ContentSettings implementation.
  virtual void OnContentBlocked(ContentSettingsType type);
  virtual void OnCookieAccessed(const GURL& url,
                                const std::string& cookie_line,
                                bool blocked_by_policy);
  virtual void OnLocalStorageAccessed(const GURL& url, bool blocked_by_policy);
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
    CannedBrowsingDataLocalStorageHelper* local_storages() const {
      return local_storages_;
    }

    CookiesTreeModel* GetCookiesTreeModel();

   private:
    DISALLOW_COPY_AND_ASSIGN(LocalSharedObjectsContainer);

    scoped_refptr<net::CookieMonster> cookies_;
    scoped_refptr<CannedBrowsingDataAppCacheHelper> appcaches_;
    scoped_refptr<CannedBrowsingDataDatabaseHelper> databases_;
    scoped_refptr<CannedBrowsingDataLocalStorageHelper> local_storages_;
  };

  // Stores which content setting types actually have blocked content.
  bool content_blocked_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores the blocked/allowed cookies.
  LocalSharedObjectsContainer allowed_local_shared_objects_;
  LocalSharedObjectsContainer blocked_local_shared_objects_;

  // Manages information about Geolocation API usage in this page.
  GeolocationSettingsState geolocation_settings_state_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabSpecificContentSettings);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
