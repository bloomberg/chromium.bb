// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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
        base::Bind(&VersionUpdaterCros::OnUpdateCheck,
                   weak_ptr_factory_.GetWeakPtr()));
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
                 weak_ptr_factory_.GetWeakPtr()));
}

VersionUpdaterCros::VersionUpdaterCros()
    : last_operation_(UpdateEngineClient::UPDATE_STATUS_IDLE),
      weak_ptr_factory_(this) {
}

VersionUpdaterCros::~VersionUpdaterCros() {
  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();
  update_engine_client->RemoveObserver(this);
}

void VersionUpdaterCros::UpdateStatusChanged(
    const UpdateEngineClient::Status& status) {
  Status my_status = UPDATED;
  int progress = 0;
  string16 message;

  // If the updater is currently idle, just show the last operation (unless it
  // was previously checking for an update -- in that case, the system is
  // up-to-date now).  See http://crbug.com/120063 for details.
  UpdateEngineClient::UpdateStatusOperation operation_to_show = status.status;
  if (status.status == UpdateEngineClient::UPDATE_STATUS_IDLE &&
      last_operation_ !=
      UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE) {
    operation_to_show = last_operation_;
  }

  switch (operation_to_show) {
    case UpdateEngineClient::UPDATE_STATUS_ERROR:
    case UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT:
      // This path previously used the FAILED status and IDS_UPGRADE_ERROR, but
      // the update engine reports errors for some conditions that shouldn't
      // actually be displayed as errors to users: http://crbug.com/146919.
      // Just use the UPDATED status instead.
      break;
    case UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      my_status = CHECKING;
      break;
    case UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
      progress = static_cast<int>(round(status.download_progress * 100));
      // Fall through.
    case UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
      my_status = UPDATING;
      break;
    case UpdateEngineClient::UPDATE_STATUS_VERIFYING:
    case UpdateEngineClient::UPDATE_STATUS_FINALIZING:
      // Once the download is finished, keep the progress at 100; it shouldn't
      // go down while the status is the same.
      progress = 100;
      my_status = UPDATING;
      break;
    case UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      my_status = NEARLY_UPDATED;
      break;
    default:
      break;
  }

  callback_.Run(my_status, progress, message);
  last_operation_ = status.status;
}

void VersionUpdaterCros::OnUpdateCheck(
    UpdateEngineClient::UpdateCheckResult result) {
  // If version updating is not implemented, this binary is the most up-to-date
  // possible with respect to automatic updating.
  if (result == UpdateEngineClient::UPDATE_RESULT_NOTIMPLEMENTED)
    callback_.Run(UPDATED, 0, string16());
}

void VersionUpdaterCros::UpdateSelectedChannel(const std::string& channel) {
  channel_callback_.Run(channel);
}
