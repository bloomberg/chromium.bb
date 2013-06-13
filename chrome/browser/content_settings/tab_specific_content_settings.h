// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/geolocation/geolocation_settings_state.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/cookies/canonical_cookie.h"

class CookiesTreeModel;
class Profile;

namespace content {
class RenderViewHost;
}

namespace net {
class CookieOptions;
}

// This class manages state about permissions, content settings, cookies and
// site data for a specific WebContents. It tracks which content was accessed
// and which content was blocked. Based on this it provides information about
// which types of content were accessed and blocked.
class TabSpecificContentSettings
    : public content::WebContentsObserver,
      public content::NotificationObserver,
      public content::WebContentsUserData<TabSpecificContentSettings> {
 public:
  // Classes that want to be notified about site data events must implement
  // this abstract class and add themselves as observer to the
  // |TabSpecificContentSettings|.
  class SiteDataObserver {
   public:
    explicit SiteDataObserver(
        TabSpecificContentSettings* tab_specific_content_settings);
    virtual ~SiteDataObserver();

    // Called whenever site data is accessed.
    virtual void OnSiteDataAccessed() = 0;

    TabSpecificContentSettings* tab_specific_content_settings() {
      return tab_specific_content_settings_;
    }

    // Called when the TabSpecificContentSettings is destroyed; nulls out
    // the local reference.
    void ContentSettingsDestroyed();

   private:
    TabSpecificContentSettings* tab_specific_content_settings_;

    DISALLOW_COPY_AND_ASSIGN(SiteDataObserver);
  };

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
                          const GURL& first_party_url,
                          const net::CookieList& cookie_list,
                          bool blocked_by_policy);

  // Called when a specific cookie in the current page was changed.
  // |blocked_by_policy| should be true, if the cookie was blocked due to the
  // user's content settings. In that case, this function should invoke
  // OnContentBlocked.
  static void CookieChanged(int render_process_id,
                            int render_view_id,
                            const GURL& url,
                            const GURL& first_party_url,
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

  // Resets the |content_blocked_| and |content_allowed_| arrays, except for
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

  // Returns whether a particular kind of content has been allowed. Currently
  // only tracks cookies.
  bool IsContentAllowed(ContentSettingsType content_type) const;

  const std::set<std::string>& BlockedResourcesForType(
      ContentSettingsType content_type) const;

  // Returns the GeolocationSettingsState that controls the
  // geolocation API usage on this page.
  const GeolocationSettingsState& geolocation_settings_state() const {
    return geolocation_settings_state_;
  }

  // Call to indicate that there is a protocol handler pending user approval.
  void set_pending_protocol_handler(const ProtocolHandler& handler) {
    pending_protocol_handler_ = handler;
  }

  const ProtocolHandler& pending_protocol_handler() const {
    return pending_protocol_handler_;
  }

  void ClearPendingProtocolHandler() {
    pending_protocol_handler_ = ProtocolHandler::EmptyProtocolHandler();
  }

  // Sets the previous protocol handler which will be replaced by the
  // pending protocol handler.
  void set_previous_protocol_handler(const ProtocolHandler& handler) {
    previous_protocol_handler_ = handler;
  }

  const ProtocolHandler& previous_protocol_handler() const {
    return previous_protocol_handler_;
  }

  // Set whether the setting for the pending handler is DEFAULT (ignore),
  // ALLOW, or DENY.
  void set_pending_protocol_handler_setting(ContentSetting setting) {
    pending_protocol_handler_setting_ = setting;
  }

  ContentSetting pending_protocol_handler_setting() const {
    return pending_protocol_handler_setting_;
  }


  // Returns a pointer to the |LocalSharedObjectsContainer| that contains all
  // allowed local shared objects like cookies, local storage, ... .
  const LocalSharedObjectsContainer& allowed_local_shared_objects() const {
    return allowed_local_shared_objects_;
  }

  // Returns a pointer to the |LocalSharedObjectsContainer| that contains all
  // blocked local shared objects like cookies, local storage, ... .
  const LocalSharedObjectsContainer& blocked_local_shared_objects() const {
    return blocked_local_shared_objects_;
  }

  bool load_plugins_link_enabled() { return load_plugins_link_enabled_; }
  void set_load_plugins_link_enabled(bool enabled) {
    load_plugins_link_enabled_ = enabled;
  }

  // Called to indicate whether access to the Pepper broker was allowed or
  // blocked.
  void SetPepperBrokerAllowed(bool allowed);

  // content::WebContentsObserver overrides.
  virtual void RenderViewForInterstitialPageCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void AppCacheAccessed(const GURL& manifest_url,
                                bool blocked_by_policy) OVERRIDE;

  // Message handlers. Public for testing.
  void OnContentBlocked(ContentSettingsType type,
                        const std::string& resource_identifier);
  void OnContentAllowed(ContentSettingsType type);

  // These methods are invoked on the UI thread by the static functions above.
  // Public for testing.
  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy);
  void OnCookieChanged(const GURL& url,
                       const GURL& first_party_url,
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

  // This method is called when a media stream is allowed.
  void OnMediaStreamAllowed();

  void OnMicrophoneAccessed();
  void OnMicrophoneAccessBlocked();
  void OnCameraAccessed();
  void OnCameraAccessBlocked();

  // Adds the given |SiteDataObserver|. The |observer| is notified when a
  // locale shared object, like for example a cookie, is accessed.
  void AddSiteDataObserver(SiteDataObserver* observer);

  // Removes the given |SiteDataObserver|.
  void RemoveSiteDataObserver(SiteDataObserver* observer);

 private:
  explicit TabSpecificContentSettings(content::WebContents* tab);
  friend class content::WebContentsUserData<TabSpecificContentSettings>;

  void AddBlockedResource(ContentSettingsType content_type,
                          const std::string& resource_identifier);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Notifies all registered |SiteDataObserver|s.
  void NotifySiteDataObservers();

  // All currently registered |SiteDataObserver|s.
  ObserverList<SiteDataObserver> observer_list_;

  // Stores which content setting types actually have blocked content.
  bool content_blocked_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores if the blocked content was messaged to the user.
  bool content_blockage_indicated_to_user_[CONTENT_SETTINGS_NUM_TYPES];

  // Stores which content setting types actually were allowed.
  bool content_allowed_[CONTENT_SETTINGS_NUM_TYPES];

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

  // The pending protocol handler, if any. This can be set if
  // registerProtocolHandler was invoked without user gesture.
  // The |IsEmpty| method will be true if no protocol handler is
  // pending registration.
  ProtocolHandler pending_protocol_handler_;

  // The previous protocol handler to be replaced by
  // the pending_protocol_handler_, if there is one. Empty if
  // there is no handler which would be replaced.
  ProtocolHandler previous_protocol_handler_;

  // The setting on the pending protocol handler registration. Persisted in case
  // the user opens the bubble and makes changes multiple times.
  ContentSetting pending_protocol_handler_setting_;

  // Stores whether the user can load blocked plugins on this page.
  bool load_plugins_link_enabled_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabSpecificContentSettings);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_TAB_SPECIFIC_CONTENT_SETTINGS_H_
