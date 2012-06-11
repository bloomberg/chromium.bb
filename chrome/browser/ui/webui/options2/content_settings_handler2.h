// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CONTENT_SETTINGS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CONTENT_SETTINGS_HANDLER2_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/pepper_flash_settings_manager.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class HostContentSettingsMap;
class ProtocolHandlerRegistry;

namespace options2 {

class ContentSettingsHandler : public OptionsPageUIHandler,
                               public PepperFlashSettingsManager::Client {
 public:
  ContentSettingsHandler();
  virtual ~ContentSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

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
  // Extends ContentSettingsType with some other types that will be also
  // displayed in the content settings UI.
  class ExContentSettingsType;
  struct ExContentSettingsTypeNameEntry;

  struct CachedPepperFlashSettings {
    CachedPepperFlashSettings();
    ~CachedPepperFlashSettings();

    PP_Flash_BrowserOperations_Permission default_permission;

    typedef std::map<std::string, PP_Flash_BrowserOperations_Permission>
        SiteMap;
    SiteMap sites;

    bool initialized;
  };

  // Functions that call into the page -----------------------------------------

  // Updates the page with the default settings (allow, ask, block, etc.)
  void UpdateSettingDefaultFromModel(const ExContentSettingsType& type);

  // Clobbers and rebuilds the specific content setting type exceptions table.
  void UpdateExceptionsViewFromModel(const ExContentSettingsType& type);
  // Clobbers and rebuilds the specific content setting type exceptions
  // OTR table.
  void UpdateOTRExceptionsViewFromModel(
      const ExContentSettingsType& type);
  // Clobbers and rebuilds all the exceptions tables in the page (both normal
  // and OTR tables).
  void UpdateAllExceptionsViewsFromModel();
  // As above, but only OTR tables.
  void UpdateAllOTRExceptionsViewsFromModel();
  // Clobbers and rebuilds just the geolocation exception table.
  void UpdateGeolocationExceptionsView();
  // Clobbers and rebuilds just the desktop notification exception table.
  void UpdateNotificationExceptionsView();
  // Clobbers and rebuilds just the Pepper Flash camera and microphone exception
  // table.
  void UpdateFlashCameraMicExceptionsView();
  // Clobbers and rebuilds an exception table that's managed by the host content
  // settings map.
  void UpdateExceptionsViewFromHostContentSettingsMap(ContentSettingsType type);
  // As above, but acts on the OTR table for the content setting type.
  void UpdateExceptionsViewFromOTRHostContentSettingsMap(
      ContentSettingsType type);
  // Updates the radio buttons for enabling / disabling handlers.
  void UpdateHandlersEnabledRadios();

  // Callbacks used by the page ------------------------------------------------

  // Sets the default value for a specific content type. |args| includes the
  // content type and a string describing the new default the user has
  // chosen.
  void SetContentFilter(const ListValue* args);

  // Removes the given row from the table. The first entry in |args| is the
  // content type, and the rest of the arguments depend on the content type
  // to be removed.
  void RemoveException(const ListValue* args);

  // Changes the value of an exception. Called after the user is done editing an
  // exception.
  void SetException(const ListValue* args);

  // Called to decide whether a given pattern is valid, or if it should be
  // rejected. Called while the user is editing an exception pattern.
  void CheckExceptionPatternValidity(const ListValue* args);

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
  std::string GetSettingDefaultFromModel(const ExContentSettingsType& type,
                                         std::string* provider_id);

  // Gets the ProtocolHandlerRegistry for the normal profile.
  ProtocolHandlerRegistry* GetProtocolHandlerRegistry();

  static ExContentSettingsType ExContentSettingsTypeFromGroupName(
      const std::string& name);
  static std::string ExContentSettingsTypeToGroupName(
      const ExContentSettingsType& type);

  // Member variables ---------------------------------------------------------

  content::NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_change_registrar_;
  scoped_ptr<PepperFlashSettingsManager> flash_settings_manager_;
  CachedPepperFlashSettings flash_cameramic_settings_;

  static const ExContentSettingsTypeNameEntry
      kExContentSettingsTypeGroupNames[];

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CONTENT_SETTINGS_HANDLER2_H_
