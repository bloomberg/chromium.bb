// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CONTENT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CONTENT_SETTINGS_HANDLER_H_
#pragma once

#include "chrome/browser/plugin_data_remover_helper.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class HostContentSettingsMap;

class ContentSettingsHandler : public OptionsPageUIHandler {
 public:
  ContentSettingsHandler();
  virtual ~ContentSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  virtual void Initialize();

  virtual void RegisterMessages();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Gets a string identifier for the group name, for use in HTML.
  static std::string ContentSettingsTypeToGroupName(ContentSettingsType type);

 private:
  // Functions that call into the page -----------------------------------------

  // Updates the page with the default settings (allow, ask, block, etc.)
  void UpdateSettingDefaultFromModel(ContentSettingsType type);
  // Updates the state of the "Clear plugin LSO data on exit" checkbox.
  void UpdateClearPluginLSOData();

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
  // Clobbers and rebuilds an exception table that's managed by the host content
  // settings map.
  void UpdateExceptionsViewFromHostContentSettingsMap(ContentSettingsType type);
  // As above, but acts on the OTR table for the content setting type.
  void UpdateExceptionsViewFromOTRHostContentSettingsMap(
      ContentSettingsType type);

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

  // Sets the global 3rd party cookies pref.
  void SetAllowThirdPartyCookies(const ListValue* args);

  // Utility functions ---------------------------------------------------------

  // Gets the HostContentSettingsMap for the normal profile.
  HostContentSettingsMap* GetContentSettingsMap();

  // Gets the HostContentSettingsMap for the incognito profile, or NULL if there
  // is no active incognito session.
  HostContentSettingsMap* GetOTRContentSettingsMap();

  // Gets the default setting in string form.
  std::string GetSettingDefaultFromModel(ContentSettingsType type);

  // Returns true if the default setting for the given content settings type
  // |type| is managed.
  bool GetDefaultSettingManagedFromModel(ContentSettingsType type);

  // Member variables ---------------------------------------------------------

  NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_change_registrar_;
  PluginDataRemoverHelper clear_plugin_lso_data_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CONTENT_SETTINGS_HANDLER_H_
