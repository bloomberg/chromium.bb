// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

using chromeos::CrosSettings;
using chromeos::DBusThreadManager;
using chromeos::UpdateEngineClient;
using chromeos::UserManager;
using chromeos::WizardController;

VersionUpdater* VersionUpdater::Create() {
  return new VersionUpdaterCros;
}

void VersionUpdaterCros::CheckForUpdate(const StatusCallback& callback) {
  callback_ = callback;

  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();
  update_engine_client->AddObserver(this);

  // Make sure that libcros is loaded and OOBE is complete.
  if (!WizardController::default_controller() ||
      WizardController::IsDeviceRegistered()) {
    update_engine_client->RequestUpdateCheck(
        UpdateEngineClient::EmptyUpdateCheckCallback());
  }
}

void VersionUpdaterCros::RelaunchBrowser() const {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void VersionUpdaterCros::SetReleaseChannel(const std::string& channel) {
  DBusThreadManager::Get()->GetUpdateEngineClient()->SetReleaseTrack(channel);
  // For local owner set the field in the policy blob too.
  if (UserManager::Get()->IsCurrentUserOwner())
    CrosSettings::Get()->SetString(chromeos::kReleaseChannel, channel);
}

void VersionUpdaterCros::GetReleaseChannel(const ChannelCallback& cb) {
  channel_callback_ = cb;

  // TODO(jhawkins): Store on this object.
  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();

  // Request the channel information. Use the observer to track the help page
  // handler and ensure it does not get deleted before the callback.
  update_engine_client->GetReleaseTrack(
      base::Bind(&VersionUpdaterCros::UpdateSelectedChannel,
                 base::Unretained(this)));
}

VersionUpdaterCros::VersionUpdaterCros() {}

VersionUpdaterCros::~VersionUpdaterCros() {
  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();
  update_engine_client->RemoveObserver(this);
}

void VersionUpdaterCros::UpdateStatusChanged(
    const UpdateEngineClient::Status& status) {
  Status my_status = UPDATED;
  int progress = 0;

  switch (status.status) {
    case UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      my_status = CHECKING;
      break;
    case UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
      progress = static_cast<int>(status.download_progress * 100.0);
      // Fall through.
    case UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
    case UpdateEngineClient::UPDATE_STATUS_VERIFYING:
    case UpdateEngineClient::UPDATE_STATUS_FINALIZING:
      my_status = UPDATING;
      break;
    case UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      my_status = NEARLY_UPDATED;
      break;
    default:
      // TODO(jhawkins): Handle error conditions.
      break;
  }

  callback_.Run(my_status, progress, string16());
}

// Callback from UpdateEngine with channel information.
void VersionUpdaterCros::UpdateSelectedChannel(const std::string& channel) {
  channel_callback_.Run(channel);
}
