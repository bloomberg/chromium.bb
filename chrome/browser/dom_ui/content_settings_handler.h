// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_CONTENT_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_CONTENT_SETTINGS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/options_ui.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

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

 private:
  void UpdateImagesExceptionsViewFromModel();
  void SetContentFilter(const Value* value);
  void SetAllowThirdPartyCookies(const Value* value);
  void RemoveExceptions(const Value* value);

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_CONTENT_SETTINGS_HANDLER_H_
