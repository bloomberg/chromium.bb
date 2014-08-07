// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "policy/policy_constants.h"

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

  // Retrieve the current policy for all users.
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it) {
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
  CheckForPublicSessionLocalePolicyChange(broker);
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

void ChromeUserSelectionScreen::CheckForPublicSessionLocalePolicyChange(
    policy::DeviceLocalAccountPolicyBroker* broker) {
  const std::string& user_id = broker->user_id();
  const policy::PolicyMap::Entry* entry =
      broker->core()->store()->policy_map().Get(policy::key::kSessionLocales);

  // Parse the list of recommended locales set by policy.
  std::vector<std::string> new_recommended_locales;
  base::ListValue const* list = NULL;
  if (entry &&
      entry->level == policy::POLICY_LEVEL_RECOMMENDED &&
      entry->value &&
      entry->value->GetAsList(&list)) {
    for (base::ListValue::const_iterator it = list->begin(); it != list->end();
         ++it) {
      std::string locale;
      if (!(*it)->GetAsString(&locale)) {
        NOTREACHED();
        new_recommended_locales.clear();
        break;
      }
      new_recommended_locales.push_back(locale);
    }
  }

  if (new_recommended_locales.empty()) {
    // There are no recommended locales.
    PublicSessionRecommendedLocaleMap::iterator it =
        public_session_recommended_locales_.find(user_id);
    if (it != public_session_recommended_locales_.end()) {
      // If there previously were recommended locales, remove them from
      // |public_session_recommended_locales_| and notify the UI.
      public_session_recommended_locales_.erase(it);
      SetPublicSessionLocales(user_id, &new_recommended_locales);
    }
    return;
  }

  // There are recommended locales.
  std::vector<std::string>& recommended_locales =
      public_session_recommended_locales_[user_id];
  if (new_recommended_locales != recommended_locales) {
    // If the list of recommended locales has changed, update
    // |public_session_recommended_locales_| and notify the UI.
    recommended_locales = new_recommended_locales;
    SetPublicSessionLocales(user_id, &new_recommended_locales);
  }
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

void ChromeUserSelectionScreen::SetPublicSessionLocales(
    const std::string& user_id,
    const std::vector<std::string>* recommended_locales) {
  if (!handler_initialized_)
    return;

  // Construct the list of available locales. This list consists of the
  // recommended locales, followed by all others.
  scoped_ptr<base::ListValue> locales =
      GetUILanguageList(recommended_locales, std::string());

  // Set the initially selected locale. If the list of recommended locales is
  // not empty, select its first entry. Otherwise, select the current UI locale.
  const std::string& default_locale = recommended_locales->empty() ?
      g_browser_process->GetApplicationLocale() : recommended_locales->front();

  // Set a flag to indicate whether the list of recommended locales contains at
  // least two entries. This is used to decide whether the public session pod
  // expands to its basic form (for zero or one recommended locales) or the
  // advanced form (two or more recommended locales).
  const bool two_or_more_recommended_locales = recommended_locales->size() >= 2;

  // Notify the UI.
  handler_->SetPublicSessionLocales(user_id,
                                    locales.Pass(),
                                    default_locale,
                                    two_or_more_recommended_locales);
}

}  // namespace chromeos
