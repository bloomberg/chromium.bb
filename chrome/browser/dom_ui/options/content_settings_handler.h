// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_CONTENT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_CONTENT_SETTINGS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/options/options_ui.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class HostContentSettingsMap;

class ContentSettingsHandler : public OptionsPageUIHandler {
 public:
  ContentSettingsHandler();
  virtual ~ContentSettingsHandler();

  // OptionsUIHandler implementation.
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
  void UpdateExceptionsDefaultFromModel(ContentSettingsType type);
  std::string GetExceptionsDefaultFromModel(ContentSettingsType type);
  void UpdateAllExceptionsViewsFromModel();
  void UpdateExceptionsViewFromModel(ContentSettingsType type);
  void SetContentFilter(const ListValue* args);
  void SetAllowThirdPartyCookies(const ListValue* args);
  void RemoveExceptions(const ListValue* args);
  void SetException(const ListValue* args);
  void CheckExceptionPatternValidity(const ListValue* args);
  HostContentSettingsMap* GetContentSettingsMap();
  HostContentSettingsMap* GetOTRContentSettingsMap();

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_CONTENT_SETTINGS_HANDLER_H_
