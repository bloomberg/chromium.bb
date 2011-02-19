// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_OPTIONS_PERSONAL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_WEBUI_OPTIONS_PERSONAL_OPTIONS_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/browser_signin.h"
#include "chrome/browser/dom_ui/options/options_ui.h"
#include "chrome/browser/sync/profile_sync_service.h"

class OptionsManagedBannerHandler;

// Chrome personal options page UI handler.
class PersonalOptionsHandler : public OptionsPageUIHandler,
                               public ProfileSyncServiceObserver,
                               public BrowserSignin::SigninDelegate {
 public:
  PersonalOptionsHandler();
  virtual ~PersonalOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged();

  // BrowserSignin::SigninDelegate implementation.
  virtual void OnLoginSuccess();
  virtual void OnLoginFailure(const GoogleServiceAuthError& error);

 private:
  void ObserveThemeChanged();
  void ShowSyncActionDialog(const ListValue* args);
  void ShowSyncLoginDialog(const ListValue* args);
  void ShowCustomizeSyncDialog(const ListValue* args);
  void ThemesReset(const ListValue* args);
#if defined(TOOLKIT_GTK)
  void ThemesSetGTK(const ListValue* args);
#endif

  // Called when the user updates the set of enabled data types to sync. |args|
  // is ignored.
  void OnPreferredDataTypesUpdated(const ListValue* args);

#if defined(OS_CHROMEOS)
  void LoadAccountPicture(const ListValue* args);
#endif

  scoped_ptr<OptionsManagedBannerHandler> banner_handler_;

  DISALLOW_COPY_AND_ASSIGN(PersonalOptionsHandler);
};

#endif  // CHROME_BROWSER_WEBUI_OPTIONS_PERSONAL_OPTIONS_HANDLER_H_
