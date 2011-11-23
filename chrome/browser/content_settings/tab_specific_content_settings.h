// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/geolocation/geolocation_settings_state.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class CannedBrowsingDataAppCacheHelper;
class CannedBrowsingDataCookieHelper;
class CannedBrowsingDataDatabaseHelper;
class CannedBrowsingDataFileSystemHelper;
class CannedBrowsingDataIndexedDBHelper;
class CannedBrowsingDataLocalStorageHelper;
class CookiesTreeModel;
class TabContents;
class Profile;

namespace net {
class CookieList;
class CookieOptions;
}

class TabSpecificContentSettings : public TabContentsObserver,
                                   public content::NotificationObserver {
 public:
  explicit TabSpecificContentSettings(TabContents* tab);

  virtual ~TabSpecificContentSettings();

  // Returns the object given a render view's id.
  static TabSpecificContentSettings* Get(int render_process_id,
                                         int render_view_id);

  // Static methods called on the UI threads.
  // Called when cookies for the given URL were read either from within the
  // current page or while loading it. |blocked_by_policy| should be true, if
  // reading cookies was blocked due to the user's content settings. In that
  // case, this function should invoke OnContentBlocked.
  static void CookiesRead(int render_process_id,
                          int render_view_id,
                          const GURL& url,
                          const net::CookieList& cookie_list,
                          bool blocked_by_policy);

  // Called when a specific cookie in the current page was changed.
  // |blocked_by_policy| should be true, if the cookie was blocked due to the
  // user's content settings. In that case, this function should invoke
  // OnContentBlocked.
  static void CookieChanged(int render_process_id,
                            int render_view_id,
                            const GURL& url,
                            const std::string& cookie_line,
                            const net::CookieOptions& options,
                            bool blocked_by_policy);

  // Called when a specific Web database in the current page was accessed. If
  // access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void WebDatabaseAccessed(int render_process_id,
                                  int render_view_id,
                                  const GURL& url,
                                  const string16& name,
                                  const string16& display_name,
                                  bool blocked_by_policy);

  // Called when a specific DOM storage area in the current page was
  // accessed. If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void DOMStorageAccessed(int render_process_id,
                                 int render_view_id,
                                 const GURL& url,
                                 bool local,
                                 bool blocked_by_policy);

  // Called when a specific indexed db factory in the current page was
  // accessed. If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void IndexedDBAccessed(int render_process_id,
                                int render_view_id,
                                const GURL& url,
                                const string16& description,
                                bool blocked_by_policy);

  // Called when a specific file system in the current page was accessed.
  // If access was blocked due to the user's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  static void FileSystemAccessed(int render_process_id,
                                 int render_view_id,
                                 const GURL& url,
                                 bool blocked_by_policy);

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
      const content::LoadCommittedDetails& details);

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

  // TabContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void AppCacheAccessed(const GURL& manifest_url,
                                bool blocked_by_policy) OVERRIDE;

  // Message handlers. Public for testing.
  void OnContentBlocked(ContentSettingsType type,
                        const std::string& resource_identifier);

  // These methods are invoked on the UI thread by the static functions above.
  // Public for testing.
  void OnCookiesRead(const GURL& url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy);
  void OnCookieChanged(const GURL& url,
                       const std::string& cookie_line,
                       const net::CookieOptions& options,
                       bool blocked_by_policy);
  void OnFileSystemAccessed(const GURL& url,
                            bool blocked_by_policy);
  void OnIndexedDBAccessed(const GURL& url,
                           const string16& description,
                           bool blocked_by_policy);
  void OnLocalStorageAccessed(const GURL& url,
                              bool local,
                              bool blocked_by_policy);
  void OnWebDatabaseAccessed(const GURL& url,
                             const string16& name,
                             const string16& display_name,
                             bool blocked_by_policy);
  void OnGeolocationPermissionSet(const GURL& requesting_frame,
                                  bool allowed);

 private:
  class LocalSharedObjectsContainer {
   public:
    explicit LocalSharedObjectsContainer(Profile* profile);
    ~LocalSharedObjectsContainer();

    // Empties the container.
    void Reset();

    CannedBrowsingDataAppCacheHelper* appcaches() const {
      return appcaches_;
    }
    CannedBrowsingDataCookieHelper* cookies() const {
      return cookies_;
    }
    CannedBrowsingDataDatabaseHelper* databases() const {
      return databases_;
    }
    CannedBrowsingDataFileSystemHelper* file_systems() const {
      return file_systems_;
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
    scoped_refptr<CannedBrowsingDataAppCacheHelper> appcaches_;
    scoped_refptr<CannedBrowsingDataCookieHelper> cookies_;
    scoped_refptr<CannedBrowsingDataDatabaseHelper> databases_;
    scoped_refptr<CannedBrowsingDataFileSystemHelper> file_systems_;
    scoped_refptr<CannedBrowsingDataIndexedDBHelper> indexed_dbs_;
    scoped_refptr<CannedBrowsingDataLocalStorageHelper> local_storages_;
    scoped_refptr<CannedBrowsingDataLocalStorageHelper> session_storages_;

    DISALLOW_COPY_AND_ASSIGN(LocalSharedObjectsContainer);
  };

  void AddBlockedResource(ContentSettingsType content_type,
                          const std::string& resource_identifier);

  void OnContentAccessed(ContentSettingsType type);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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

  // The profile of the tab.
  Profile* profile_;

  // Stores the blocked/allowed cookies.
  LocalSharedObjectsContainer allowed_local_shared_objects_;
  LocalSharedObjectsContainer blocked_local_shared_objects_;

  // Manages information about Geolocation API usage in this page.
  GeolocationSettingsState geolocation_settings_state_;

  // Stores whether the user can load blocked plugins on this page.
  bool load_plugins_link_enabled_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabSpecificContentSettings);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
