// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_SUPERVISED_USER_CREATE_CONFIRM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_SUPERVISED_USER_CREATE_CONFIRM_HANDLER_H_

#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class DictionaryValue;
}

namespace options {

// Handler for the confirmation dialog after successful creation of a supervised
// user.
class SupervisedUserCreateConfirmHandler : public OptionsPageUIHandler {
 public:
  SupervisedUserCreateConfirmHandler();
  ~SupervisedUserCreateConfirmHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // An observer for any changes to Profiles in the ProfileInfoCache so that
  // this dialog can be updated or closed.
  class ProfileUpdateObserver;

  // Callback for the "switchToProfile" message.
  // Opens a new window for the profile.
  // |args| is of the form [ {string} profileFilePath ]
  void SwitchToProfile(const base::ListValue* args);

  // Observes the ProfileInfoCache and gets notified when a profile has been
  // modified, so that the dialog can be updated or closed.
  scoped_ptr<ProfileUpdateObserver> profile_info_cache_observer_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserCreateConfirmHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_SUPERVISED_USER_CREATE_CONFIRM_HANDLER_H_
