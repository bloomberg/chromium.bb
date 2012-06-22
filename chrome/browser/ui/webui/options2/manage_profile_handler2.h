// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_MANAGE_PROFILE_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_MANAGE_PROFILE_HANDLER2_H_
#pragma once

#include "chrome/browser/ui/webui/options2/options_ui2.h"

namespace options2 {

// Chrome personal stuff profiles manage overlay UI handler.
class ManageProfileHandler : public OptionsPageUIHandler {
 public:
  ManageProfileHandler();
  virtual ~ManageProfileHandler();

  // OptionsPageUIHandler:
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;

  // WebUIMessageHandler:
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Callback for the "requestDefaultProfileIcons" message.
  // Sends the array of default profile icon URLs to WebUI.
  // |args| is of the form: [ {string} iconURL ]
  void RequestDefaultProfileIcons(const base::ListValue* args);

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

  // Callback for the 'profileIconSelectionChanged' message. Used to update the
  // name in the manager profile dialog based on the selected icon.
  void ProfileIconSelectionChanged(const base::ListValue* args);

  // Send all profile icons to the overlay.
  void SendProfileIcons();

  // URL for the current profile's GAIA picture.
  std::string gaia_picture_url_;

  DISALLOW_COPY_AND_ASSIGN(ManageProfileHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_MANAGE_PROFILE_HANDLER2_H_
