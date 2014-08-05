// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_CHROME_USER_SELECTION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_CHROME_USER_SELECTION_SCREEN_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"

namespace chromeos {

class ChromeUserSelectionScreen
    : public UserSelectionScreen,
      public policy::DeviceLocalAccountPolicyService::Observer {
 public:
  ChromeUserSelectionScreen();
  virtual ~ChromeUserSelectionScreen();

  // UserSelectionScreen:
  virtual void Init(const user_manager::UserList& users,
                    bool show_guest) OVERRIDE;
  virtual void SendUserList() OVERRIDE;

  // policy::DeviceLocalAccountPolicyService::Observer:
  virtual void OnPolicyUpdated(const std::string& user_id) OVERRIDE;
  virtual void OnDeviceLocalAccountsChanged() OVERRIDE;

 private:
  // Check whether the display name set by policy for a public session has
  // changed and if so, notify the UI.
  void CheckForPublicSessionDisplayNameChange(
      policy::DeviceLocalAccountPolicyBroker* broker);

  // Check whether the list of recommended locales set by policy for a public
  // session has changed and if so, notify the UI.
  void CheckForPublicSessionLocalePolicyChange(
      policy::DeviceLocalAccountPolicyBroker* broker);

  // Notify the UI that the display name for a public session has changed,
  // taking the display name from the |User| owned by |UserManager|.
  void SetPublicSessionDisplayName(const std::string& user_id);

  // Send an updated list of locales for a public session to the UI, consisting
  // of the |recommended_locales| followed by all other available locales.
  void SetPublicSessionLocales(
      const std::string& user_id,
      const std::vector<std::string>* recommended_locales);

  bool handler_initialized_;

  policy::DeviceLocalAccountPolicyService* device_local_account_policy_service_;

  // Map from public session user IDs to their display names set by policy.
  typedef std::map<std::string, std::string> DisplayNamesMap;
  DisplayNamesMap public_session_display_names_;

  base::WeakPtrFactory<ChromeUserSelectionScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeUserSelectionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_CHROME_USER_SELECTION_SCREEN_H_
