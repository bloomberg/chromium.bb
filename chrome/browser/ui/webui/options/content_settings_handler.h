// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CONTENT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CONTENT_SETTINGS_HANDLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/scoped_observer.h"
#include "chrome/browser/pepper_flash_settings_manager.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/options/pepper_flash_content_settings_utils.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class HostContentSettingsMap;
class ProtocolHandlerRegistry;

namespace options {

class ContentSettingsHandler : public OptionsPageUIHandler,
                               public content_settings::Observer,
                               public content::NotificationObserver,
                               public PepperFlashSettingsManager::Client {
 public:
  ContentSettingsHandler();
  virtual ~ContentSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // content_settings::Observer implementation.
  virtual void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      std::string resource_identifier) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // PepperFlashSettingsManager::Client implementation.
  virtual void OnGetPermissionSettingsCompleted(
      uint32 request_id,
      bool success,
      PP_Flash_BrowserOperations_Permission default_permission,
      const ppapi::FlashSiteSettings& sites) OVERRIDE;

  // Gets a string identifier for the group name, for use in HTML.
  static std::string ContentSettingsTypeToGroupName(ContentSettingsType type);

 private:
  // Used to determine whether we should show links to Flash camera and
  // microphone settings.
  struct MediaSettingsInfo {
    MediaSettingsInfo();
    ~MediaSettingsInfo();

    // Cached Pepper Flash settings.
    ContentSetting flash_default_setting;
    MediaExceptions flash_exceptions;
    bool flash_settings_initialized;
    uint32_t last_flash_refresh_request_id;

    // Whether the links to Flash settings pages are showed.
    bool show_flash_default_link;
    bool show_flash_exceptions_link;

    // Cached Chrome media settings.
    ContentSetting default_setting;
    bool policy_disable_audio;
    bool policy_disable_video;
    bool default_setting_initialized;
    MediaExceptions exceptions;
    bool exceptions_initialized;
  };

  // Used by ShowFlashMediaLink() to specify which link to show/hide.
  enum LinkType {
    DEFAULT_SETTING = 0,
    EXCEPTIONS,
  };

  // Functions that call into the page -----------------------------------------

  // Updates the page with the default settings (allow, ask, block, etc.)
  void UpdateSettingDefaultFromModel(ContentSettingsType type);

  // Updates the media radio buttons according to the enabled split prefs.
  void UpdateMediaSettingsView();

  // Clobbers and rebuilds the specific content setting type exceptions table.
  void UpdateExceptionsViewFromModel(ContentSettingsType type);

  // Clobbers and rebuilds the specific content setting type exceptions
  // OTR table.
  void UpdateOTRExceptionsViewFromModel(ContentSettingsType type);

  // Clobbers and rebuilds all the exceptions tables in the page (both normal
  // and OTR tables).
  void UpdateAllExceptionsViewsFromModel();

  // As above, but only OTR tables.
  void UpdateAllOTRExceptionsViewsFromModel();

  // Clobbers and rebuilds just the geolocation exception table.
  void UpdateGeolocationExceptionsView();

  // Clobbers and rebuilds just the desktop notification exception table.
  void UpdateNotificationExceptionsView();

  // Clobbers and rebuilds just the Media device exception table.
  void UpdateMediaExceptionsView();

  // Clobbers and rebuilds just the MIDI SysEx exception table.
  void UpdateMIDISysExExceptionsView();

  // Clobbers and rebuilds just the zoom levels exception table.
  void UpdateZoomLevelsExceptionsView();

  // Clobbers and rebuilds an exception table that's managed by the host content
  // settings map.
  void UpdateExceptionsViewFromHostContentSettingsMap(ContentSettingsType type);

  // As above, but acts on the OTR table for the content setting type.
  void UpdateExceptionsViewFromOTRHostContentSettingsMap(
      ContentSettingsType type);

  // Updates the radio buttons for enabling / disabling handlers.
  void UpdateHandlersEnabledRadios();

  // Removes one geolocation exception. |args| contains the parameters passed to
  // RemoveException().
  void RemoveGeolocationException(const base::ListValue* args);

  // Removes one notification exception. |args| contains the parameters passed
  // to RemoveException().
  void RemoveNotificationException(const base::ListValue* args);

  // Removes one media camera and microphone exception. |args| contains the
  // parameters passed to RemoveException().
  void RemoveMediaException(const base::ListValue* args);

  // Removes one exception of |type| from the host content settings map. |args|
  // contains the parameters passed to RemoveException().
  void RemoveExceptionFromHostContentSettingsMap(
      const base::ListValue* args,
      ContentSettingsType type);

  // Removes one zoom level exception. |args| contains the parameters passed to
  // RemoveException().
  void RemoveZoomLevelException(const base::ListValue* args);

  // Callbacks used by the page ------------------------------------------------

  // Sets the default value for a specific content type. |args| includes the
  // content type and a string describing the new default the user has
  // chosen.
  void SetContentFilter(const base::ListValue* args);

  // Removes the given row from the table. The first entry in |args| is the
  // content type, and the rest of the arguments depend on the content type
  // to be removed.
  void RemoveException(const base::ListValue* args);

  // Changes the value of an exception. Called after the user is done editing an
  // exception.
  void SetException(const base::ListValue* args);

  // Called to decide whether a given pattern is valid, or if it should be
  // rejected. Called while the user is editing an exception pattern.
  void CheckExceptionPatternValidity(const base::ListValue* args);

  // Utility functions ---------------------------------------------------------

  // Applies content settings whitelists to reduce breakage / user confusion.
  void ApplyWhitelist(ContentSettingsType content_type,
                      ContentSetting default_setting);

  // Gets the HostContentSettingsMap for the normal profile.
  HostContentSettingsMap* GetContentSettingsMap();

  // Gets the HostContentSettingsMap for the incognito profile, or NULL if there
  // is no active incognito session.
  HostContentSettingsMap* GetOTRContentSettingsMap();

  // Gets the default setting in string form. If |provider_id| is not NULL, the
  // id of the provider which provided the default setting is assigned to it.
  std::string GetSettingDefaultFromModel(ContentSettingsType type,
                                         std::string* provider_id);

  // Gets the ProtocolHandlerRegistry for the normal profile.
  ProtocolHandlerRegistry* GetProtocolHandlerRegistry();

  void RefreshFlashMediaSettings();

  // Fills in |exceptions| with Values for the given |type| from |map|.
  void GetExceptionsFromHostContentSettingsMap(
      const HostContentSettingsMap* map,
      ContentSettingsType type,
      base::ListValue* exceptions);

  void OnPepperFlashPrefChanged();

  // content::HostZoomMap subscription.
  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  void ShowFlashMediaLink(LinkType link_type, bool show);

  void UpdateFlashMediaLinksVisibility();

  void UpdateProtectedContentExceptionsButton();

  // Member variables ---------------------------------------------------------

  content::NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_change_registrar_;
  scoped_ptr<PepperFlashSettingsManager> flash_settings_manager_;
  MediaSettingsInfo media_settings_;
  scoped_ptr<content::HostZoomMap::Subscription> host_zoom_map_subscription_;
  ScopedObserver<HostContentSettingsMap, content_settings::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CONTENT_SETTINGS_HANDLER_H_
