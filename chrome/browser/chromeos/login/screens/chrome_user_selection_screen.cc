// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace chromeos {

ChromeUserSelectionScreen::ChromeUserSelectionScreen()
    : handler_initialized_(false),
      weak_factory_(this) {
  device_local_account_policy_service_ = g_browser_process->platform_part()->
      browser_policy_connector_chromeos()->GetDeviceLocalAccountPolicyService();
  device_local_account_policy_service_->AddObserver(this);
}

ChromeUserSelectionScreen::~ChromeUserSelectionScreen() {
  device_local_account_policy_service_->RemoveObserver(this);
}

void ChromeUserSelectionScreen::Init(const user_manager::UserList& users,
                                     bool show_guest) {
  UserSelectionScreen::Init(users, show_guest);

  // Retrieve the current policy for |users_|.
  for (user_manager::UserList::const_iterator it = users_.begin();
       it != users_.end(); ++it) {
    if ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
      OnPolicyUpdated((*it)->GetUserID());
  }
}

void ChromeUserSelectionScreen::SendUserList() {
  UserSelectionScreen::SendUserList();
  handler_initialized_ = true;
}

void ChromeUserSelectionScreen::OnPolicyUpdated(const std::string& user_id) {
  policy::DeviceLocalAccountPolicyBroker* broker =
      device_local_account_policy_service_->GetBrokerForUser(user_id);
  if (!broker)
    return;

  CheckForPublicSessionDisplayNameChange(broker);
}

void ChromeUserSelectionScreen::OnDeviceLocalAccountsChanged() {
  // Nothing to do here. When the list of device-local accounts changes, the
  // entire UI is reloaded.
}

void ChromeUserSelectionScreen::CheckForPublicSessionDisplayNameChange(
    policy::DeviceLocalAccountPolicyBroker* broker) {
  const std::string& user_id = broker->user_id();
  const std::string& display_name = broker->GetDisplayName();
  if (display_name == public_session_display_names_[user_id])
    return;

  public_session_display_names_[user_id] = display_name;

  if (!handler_initialized_)
    return;

  if (!display_name.empty()) {
    // If a new display name was set by policy, notify the UI about it.
    handler_->SetPublicSessionDisplayName(user_id, display_name);
    return;
  }

  // When no display name is set by policy, the |User|, owned by |UserManager|,
  // decides what display name to use. However, the order in which |UserManager|
  // and |this| are informed of the display name change is undefined. Post a
  // task that will update the UI after the UserManager is guaranteed to have
  // been informed of the change.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ChromeUserSelectionScreen::SetPublicSessionDisplayName,
                 weak_factory_.GetWeakPtr(),
                 user_id));
}

void ChromeUserSelectionScreen::SetPublicSessionDisplayName(
    const std::string& user_id) {
  const user_manager::User* user = UserManager::Get()->FindUser(user_id);
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return;

  handler_->SetPublicSessionDisplayName(
      user_id,
      base::UTF16ToUTF8(user->GetDisplayName()));
}

}  // namespace chromeos
