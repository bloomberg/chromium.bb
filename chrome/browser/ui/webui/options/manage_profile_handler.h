// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGE_PROFILE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGE_PROFILE_HANDLER_H_
#pragma once

#include "chrome/browser/ui/webui/options/options_ui.h"

// Chrome personal stuff profiles manage overlay UI handler.
class ManageProfileHandler : public OptionsPageUIHandler {
 public:
  ManageProfileHandler();
  virtual ~ManageProfileHandler();

  // OptionsPageUIHandler:
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings);
  virtual void Initialize();

  // WebUIMessageHandler:
  virtual void RegisterMessages();

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Send the array of default profile icon URLs to WebUI.
  void InitializeDefaultProfileIcons();

  // Sends an object to WebUI of the form:
  //   profileNames = {
  //     "Profile Name 1": true,
  //     "Profile Name 2": true,
  //     ...
  //   };
  // This is used to detect duplicate profile names.
  void SendProfileNames();

  // Callback for the "setProfileNameAndIcon" message. Sets the name and icon
  // of a given profile.
  // |args| is of the form: [
  //   /*string*/ profileFilePath,
  //   /*string*/ newProfileName,
  //   /*string*/ newProfileIconURL
  // ]
  void SetProfileNameAndIcon(const base::ListValue* args);

  // Callback for the "deleteProfile" message. Deletes the given profile.
  // |args| is of the form: [ {string} profileFilePath ]
  void DeleteProfile(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(ManageProfileHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGE_PROFILE_HANDLER_H_

