// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/network_connect.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace chromeos {
namespace network_connect {

namespace {

void ActivateCellular(chromeos::CellularNetwork* cellular) {
  DCHECK(cellular);
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (cros->CellularDeviceUsesDirectActivation()) {
    cellular->StartActivation();
  } else {
    ash::Shell::GetInstance()->delegate()->OpenMobileSetup(
        cellular->service_path());
  }
}

void DoConnect(Network* network, gfx::NativeWindow parent_window) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (network->type() == TYPE_VPN) {
    VirtualNetwork* vpn = static_cast<VirtualNetwork*>(network);
    if (vpn->NeedMoreInfoToConnect()) {
      // Show the connection UI if info for a field is missing.
      NetworkConfigView::Show(vpn, parent_window);
    } else {
      cros->ConnectToVirtualNetwork(vpn);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
    }
  } else if (network->type() == TYPE_WIFI) {
    WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
    if (wifi->IsPassphraseRequired()) {
      // Show the connection UI if we require a passphrase.
      NetworkConfigView::Show(wifi, parent_window);
    } else {
      cros->ConnectToWifiNetwork(wifi);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
    }
  } else if (network->type() == TYPE_WIMAX) {
    WimaxNetwork* wimax = static_cast<WimaxNetwork*>(network);
    if (wimax->passphrase_required()) {
      // Show the connection UI if we require a passphrase.
      NetworkConfigView::Show(wimax, parent_window);
    } else {
      cros->ConnectToWimaxNetwork(wimax);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
    }
  } else if (network->type() == TYPE_CELLULAR) {
    CellularNetwork* cellular = static_cast<CellularNetwork*>(network);
    if (cellular->activation_state() != ACTIVATION_STATE_ACTIVATED ||
        cellular->out_of_credits()) {
      ActivateCellular(cellular);
    } else {
      cros->ConnectToCellularNetwork(cellular);
    }
  }
}

}  // namespace

ConnectResult ConnectToNetwork(const std::string& service_path,
                               gfx::NativeWindow parent_window) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  Network* network = cros->FindNetworkByPath(service_path);
  if (!network)
    return NETWORK_NOT_FOUND;

  if (network->connecting_or_connected())
    return CONNECT_NOT_STARTED;

  if (network->type() == TYPE_ETHERNET)
    return CONNECT_NOT_STARTED;  // Normally this shouldn't happen

  if (network->type() == TYPE_WIFI) {
    WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
    wifi->SetEnrollmentDelegate(
        chromeos::CreateEnrollmentDelegate(
            parent_window,
            wifi->name(),
            ProfileManager::GetLastUsedProfile()));
    wifi->AttemptConnection(base::Bind(&DoConnect, wifi, parent_window));
    return CONNECT_STARTED;
  }

  if (network->type() == TYPE_WIMAX) {
    WimaxNetwork* wimax = static_cast<WimaxNetwork*>(network);
    wimax->AttemptConnection(base::Bind(&DoConnect, wimax, parent_window));
    return CONNECT_STARTED;
  }

  if (network->type() == TYPE_CELLULAR) {
    CellularNetwork* cellular = static_cast<CellularNetwork*>(network);
    if (cellular->NeedsActivation() || cellular->out_of_credits()) {
      ActivateCellular(cellular);
      return CONNECT_STARTED;
    }
    if (cellular->activation_state() == ACTIVATION_STATE_ACTIVATING)
      return CONNECT_NOT_STARTED;
    cros->ConnectToCellularNetwork(cellular);
    return CONNECT_STARTED;
  }

  if (network->type() == TYPE_VPN) {
    VirtualNetwork* vpn = static_cast<VirtualNetwork*>(network);
    vpn->SetEnrollmentDelegate(
        chromeos::CreateEnrollmentDelegate(
            parent_window,
            vpn->name(),
            ProfileManager::GetLastUsedProfile()));
    vpn->AttemptConnection(base::Bind(&DoConnect, vpn, parent_window));
    return CONNECT_STARTED;
  }

  NOTREACHED();
  return CONNECT_NOT_STARTED;
}

}  // namespace network_connect
}  // namespace chromeos
