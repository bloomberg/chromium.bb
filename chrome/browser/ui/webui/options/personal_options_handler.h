// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_PERSONAL_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_PERSONAL_OPTIONS_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#if defined(OS_CHROMEOS)
#include "content/public/browser/notification_registrar.h"
#endif

// Chrome personal options page UI handler.
class PersonalOptionsHandler : public OptionsPageUIHandler,
                               public ProfileSyncServiceObserver {
 public:
  PersonalOptionsHandler();
  virtual ~PersonalOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

 private:
  void ObserveThemeChanged();
  void ThemesReset(const ListValue* args);
#if defined(TOOLKIT_GTK)
  void ThemesSetGTK(const ListValue* args);
#endif

#if defined(OS_CHROMEOS)
  void UpdateAccountPicture();
  content::NotificationRegistrar registrar_;
#endif

  // Sends an array of Profile objects to javascript.
  // Each object is of the form:
  //   profileInfo = {
  //     name: "Profile Name",
  //     iconURL: "chrome://path/to/icon/image",
  //     filePath: "/path/to/profile/data/on/disk",
  //     isCurrentProfile: false
  //   };
  void SendProfilesInfo();

  // Asynchronously opens a new browser window to create a new profile.
  // |args| is not used.
  void CreateProfile(const ListValue* args);

  // True if the multiprofiles switch is enabled.
  bool multiprofile_;

  DISALLOW_COPY_AND_ASSIGN(PersonalOptionsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_PERSONAL_OPTIONS_HANDLER_H_
