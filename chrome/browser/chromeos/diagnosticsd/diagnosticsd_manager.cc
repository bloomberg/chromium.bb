// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_bridge.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

DiagnosticsdManager* g_diagnosticsd_manager_instance = nullptr;

class DiagnosticsdManagerDelegateImpl final
    : public DiagnosticsdManager::Delegate {
 public:
  DiagnosticsdManagerDelegateImpl();
  ~DiagnosticsdManagerDelegateImpl() override;

  // Delegate overrides:
  std::unique_ptr<DiagnosticsdBridge> CreateDiagnosticsdBridge() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdManagerDelegateImpl);
};

DiagnosticsdManagerDelegateImpl::DiagnosticsdManagerDelegateImpl() = default;

DiagnosticsdManagerDelegateImpl::~DiagnosticsdManagerDelegateImpl() = default;

std::unique_ptr<DiagnosticsdBridge>
DiagnosticsdManagerDelegateImpl::CreateDiagnosticsdBridge() {
  return std::make_unique<DiagnosticsdBridge>(
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory());
}

// Returns true if only affiliated users are logged-in.
bool AreOnlyAffiliatedUsersLoggedIn() {
  const user_manager::UserList logged_in_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::User* user : logged_in_users) {
    if (!user->IsAffiliated()) {
      return false;
    }
  }
  return true;
}

}  // namespace

DiagnosticsdManager::Delegate::~Delegate() = default;

// static
DiagnosticsdManager* DiagnosticsdManager::Get() {
  return g_diagnosticsd_manager_instance;
}

DiagnosticsdManager::DiagnosticsdManager()
    : DiagnosticsdManager(std::make_unique<DiagnosticsdManagerDelegateImpl>()) {
}

DiagnosticsdManager::DiagnosticsdManager(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      callback_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  DCHECK(!g_diagnosticsd_manager_instance);
  g_diagnosticsd_manager_instance = this;
  wilco_dtc_allowed_observer_ = CrosSettings::Get()->AddSettingsObserver(
      kDeviceWilcoDtcAllowed,
      base::BindRepeating(&DiagnosticsdManager::StartOrStopWilcoDtc,
                          weak_ptr_factory_.GetWeakPtr()));

  session_manager::SessionManager::Get()->AddObserver(this);

  StartOrStopWilcoDtc();
}

DiagnosticsdManager::~DiagnosticsdManager() {
  DCHECK_EQ(g_diagnosticsd_manager_instance, this);
  g_diagnosticsd_manager_instance = nullptr;

  // The destruction may mean that non-affiliated user is logging out.
  StartOrStopWilcoDtc();

  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void DiagnosticsdManager::SetConfigurationData(
    std::unique_ptr<std::string> data) {
  configuration_data_ = std::move(data);

  if (!diagnosticsd_bridge_) {
    VLOG(0) << "Cannot send notification - no bridge to the daemon";
    return;
  }
  diagnosticsd_bridge_->SetConfigurationData(configuration_data_.get());

  diagnosticsd::mojom::DiagnosticsdServiceProxy* const diagnosticsd_mojo_proxy =
      diagnosticsd_bridge_->diagnosticsd_service_mojo_proxy();
  if (!diagnosticsd_mojo_proxy) {
    VLOG(0) << "Cannot send message - Mojo connection to the daemon isn't "
               "bootstrapped yet";
    return;
  }
  diagnosticsd_mojo_proxy->NotifyConfigurationDataChanged();
}

void DiagnosticsdManager::OnSessionStateChanged() {
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  // The user is logged-in and the affiliation is set.
  if (session_state == session_manager::SessionState::ACTIVE)
    StartOrStopWilcoDtc();
}

void DiagnosticsdManager::StartOrStopWilcoDtc() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
  bool wilco_dtc_allowed;
  // Start wilco DTC support services only if logged-in users are affiliated
  // and the wilco DTC is allowed by policy.
  if (CrosSettings::Get()->GetBoolean(kDeviceWilcoDtcAllowed,
                                      &wilco_dtc_allowed) &&
      wilco_dtc_allowed && AreOnlyAffiliatedUsersLoggedIn()) {
    StartWilcoDtc(base::BindOnce(&DiagnosticsdManager::OnStartWilcoDtc,
                                 callback_weak_ptr_factory_.GetWeakPtr()));
  } else {
    StopWilcoDtc(base::BindOnce(&DiagnosticsdManager::OnStopWilcoDtc,
                                callback_weak_ptr_factory_.GetWeakPtr()));
  }
}

void DiagnosticsdManager::StartWilcoDtc(WilcoDtcCallback callback) {
  VLOG(1) << "Starting wilco DTC";
  UpstartClient::Get()->StartWilcoDtcService(std::move(callback));
}

void DiagnosticsdManager::StopWilcoDtc(WilcoDtcCallback callback) {
  VLOG(1) << "Stopping wilco DTC";
  UpstartClient::Get()->StopWilcoDtcService(std::move(callback));
}

void DiagnosticsdManager::OnStartWilcoDtc(bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to start the wilco DTC";
  } else {
    VLOG(1) << "Wilco DTC started";
    if (!diagnosticsd_bridge_)
      diagnosticsd_bridge_ = delegate_->CreateDiagnosticsdBridge();
    DCHECK(diagnosticsd_bridge_);

    // Once the bridge is created, notify it about an available configuration
    // data blob.
    diagnosticsd_bridge_->SetConfigurationData(configuration_data_.get());
  }
}

void DiagnosticsdManager::OnStopWilcoDtc(bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to stop wilco DTC";
  } else {
    VLOG(1) << "Wilco DTC stopped";
    diagnosticsd_bridge_.reset();
  }
}

}  // namespace chromeos
