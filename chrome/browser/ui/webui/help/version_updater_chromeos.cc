// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/web_contents.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::CrosSettings;
using chromeos::DBusThreadManager;
using chromeos::OwnerSettingsServiceChromeOS;
using chromeos::OwnerSettingsServiceChromeOSFactory;
using chromeos::UpdateEngineClient;
using chromeos::WizardController;

namespace {

// Network status in the context of device update.
enum NetworkStatus {
  // It's allowed in device policy to use current network for update.
  NETWORK_STATUS_ALLOWED = 0,
  // It's disallowed in device policy to use current network for update.
  NETWORK_STATUS_DISALLOWED,
  // Device is in offline state.
  NETWORK_STATUS_OFFLINE
};

const bool kDefaultAutoUpdateDisabled = false;

NetworkStatus GetNetworkStatus(const chromeos::NetworkState* network) {
  if (!network || !network->IsConnectedState())  // Offline state.
    return NETWORK_STATUS_OFFLINE;

  // The connection type checking strategy must be the same as the one
  // used in update engine.
  if (network->type() == shill::kTypeBluetooth)
    return NETWORK_STATUS_DISALLOWED;
  if (network->type() == shill::kTypeCellular &&
      !help_utils_chromeos::IsUpdateOverCellularAllowed()) {
    return NETWORK_STATUS_DISALLOWED;
  }
  return NETWORK_STATUS_ALLOWED;
}

// Returns true if auto-update is disabled by the system administrator.
bool IsAutoUpdateDisabled() {
  bool update_disabled = kDefaultAutoUpdateDisabled;
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  if (!settings)
    return update_disabled;
  const base::Value* update_disabled_value =
      settings->GetPref(chromeos::kUpdateDisabled);
  if (update_disabled_value)
    CHECK(update_disabled_value->GetAsBoolean(&update_disabled));
  return update_disabled;
}

// Returns whether an update is allowed. If not, it calls the callback with
// the appropriate status.
bool EnsureCanUpdate(const VersionUpdater::StatusCallback& callback) {
  if (IsAutoUpdateDisabled()) {
    callback.Run(VersionUpdater::FAILED, 0,
                 l10n_util::GetStringUTF16(IDS_UPGRADE_DISABLED_BY_POLICY));
    return false;
  }

  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  const chromeos::NetworkState* network =
      network_state_handler->DefaultNetwork();

  // Don't allow an update if we're currently offline or connected
  // to a network for which updates are disallowed.
  NetworkStatus status = GetNetworkStatus(network);
  if (status == NETWORK_STATUS_OFFLINE) {
    callback.Run(VersionUpdater::FAILED_OFFLINE, 0,
                  l10n_util::GetStringUTF16(IDS_UPGRADE_OFFLINE));
    return false;
  } else if (status == NETWORK_STATUS_DISALLOWED) {
    base::string16 message =
        l10n_util::GetStringFUTF16(
            IDS_UPGRADE_DISALLOWED,
            help_utils_chromeos::GetConnectionTypeAsUTF16(network->type()));
    callback.Run(VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED, 0, message);
    return false;
  }

  return true;
}

}  // namespace

VersionUpdater* VersionUpdater::Create(content::WebContents* web_contents) {
  return new VersionUpdaterCros(web_contents);
}

void VersionUpdaterCros::GetUpdateStatus(const StatusCallback& callback) {
  callback_ = callback;

  if (!EnsureCanUpdate(callback))
    return;

  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();
  if (!update_engine_client->HasObserver(this))
    update_engine_client->AddObserver(this);

  this->UpdateStatusChanged(
      DBusThreadManager::Get()->GetUpdateEngineClient()->GetLastStatus());
}

void VersionUpdaterCros::CheckForUpdate(const StatusCallback& callback,
                                        const PromoteCallback&) {
  callback_ = callback;

  if (!EnsureCanUpdate(callback))
    return;

  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();
  if (!update_engine_client->HasObserver(this))
    update_engine_client->AddObserver(this);

  if (update_engine_client->GetLastStatus().status !=
      UpdateEngineClient::UPDATE_STATUS_IDLE) {
    check_for_update_when_idle_ = true;
    return;
  }
  check_for_update_when_idle_ = false;

  // Make sure that libcros is loaded and OOBE is complete.
  if (!WizardController::default_controller() ||
      chromeos::StartupUtils::IsDeviceRegistered()) {
    update_engine_client->RequestUpdateCheck(base::Bind(
        &VersionUpdaterCros::OnUpdateCheck, weak_ptr_factory_.GetWeakPtr()));
  }
}

void VersionUpdaterCros::RelaunchBrowser() const {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void VersionUpdaterCros::SetChannel(const std::string& channel,
                                    bool is_powerwash_allowed) {
  OwnerSettingsServiceChromeOS* service =
      context_
          ? OwnerSettingsServiceChromeOSFactory::GetInstance()
                ->GetForBrowserContext(context_)
          : nullptr;
  // For local owner set the field in the policy blob.
  if (service)
    service->SetString(chromeos::kReleaseChannel, channel);
  DBusThreadManager::Get()->GetUpdateEngineClient()->
      SetChannel(channel, is_powerwash_allowed);
}

void VersionUpdaterCros::GetChannel(bool get_current_channel,
                                    const ChannelCallback& cb) {
  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();

  // Request the channel information.
  update_engine_client->GetChannel(get_current_channel, cb);
}

VersionUpdaterCros::VersionUpdaterCros(content::WebContents* web_contents)
    : context_(web_contents ? web_contents->GetBrowserContext() : nullptr),
      last_operation_(UpdateEngineClient::UPDATE_STATUS_IDLE),
      check_for_update_when_idle_(false),
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
  base::string16 message;

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
    case UpdateEngineClient::UPDATE_STATUS_ATTEMPTING_ROLLBACK:
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

  if (check_for_update_when_idle_ &&
      status.status == UpdateEngineClient::UPDATE_STATUS_IDLE) {
    CheckForUpdate(callback_, VersionUpdater::PromoteCallback());
  }
}

void VersionUpdaterCros::OnUpdateCheck(
    UpdateEngineClient::UpdateCheckResult result) {
  // If version updating is not implemented, this binary is the most up-to-date
  // possible with respect to automatic updating.
  if (result == UpdateEngineClient::UPDATE_RESULT_NOTIMPLEMENTED)
    callback_.Run(UPDATED, 0, base::string16());
}
