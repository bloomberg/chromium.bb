// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/network_connect.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/chromeos/network/network_connect.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace network_connect {

namespace {

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
      ActivateCellular(cellular->service_path());
    } else {
      cros->ConnectToCellularNetwork(cellular);
    }
  }
}

}  // namespace

void ActivateCellular(const std::string& service_path) {
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (!cros->CellularDeviceUsesDirectActivation()) {
    // For non direct activation, show the mobile setup dialog which can be
    // used to activate the network.
    ShowMobileSetup(service_path);
    return;
  }
  chromeos::CellularNetwork* cellular =
      cros->FindCellularNetworkByPath(service_path);
  if (!cellular)
    return;
  if (cellular->activation_state() != chromeos::ACTIVATION_STATE_ACTIVATED)
    cellular->StartActivation();
  return;
}

void ShowMobileSetup(const std::string& service_path) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  const CellularNetwork* cellular =
      cros->FindCellularNetworkByPath(service_path);
  if (cellular && !cellular->activated() &&
      cellular->activate_over_non_cellular_network() &&
      (!cros->connected_network() || !cros->connected_network()->online())) {
    NetworkTechnology technology = cellular->network_technology();
    ash::NetworkObserver::NetworkType network_type =
        (technology == chromeos::NETWORK_TECHNOLOGY_LTE ||
         technology == chromeos::NETWORK_TECHNOLOGY_LTE_ADVANCED)
        ? ash::NetworkObserver::NETWORK_CELLULAR_LTE
        : ash::NetworkObserver::NETWORK_CELLULAR;
    ash::Shell::GetInstance()->system_tray_notifier()->NotifySetNetworkMessage(
        NULL,
        ash::NetworkObserver::ERROR_CONNECT_FAILED,
        network_type,
        l10n_util::GetStringUTF16(IDS_NETWORK_ACTIVATION_ERROR_TITLE),
        l10n_util::GetStringFUTF16(IDS_NETWORK_ACTIVATION_NEEDS_CONNECTION,
                                   UTF8ToUTF16((cellular->name()))),
        std::vector<string16>());
    return;
  }
  MobileSetupDialog::Show(service_path);
}

ConnectResult ConnectToNetwork(const std::string& service_path,
                               gfx::NativeWindow parent_window) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kUseNewNetworkConnectionHandler)) {
    ash::network_connect::ConnectToNetwork(service_path);
    return CONNECT_STARTED;
  }

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  Network* network = cros->FindNetworkByPath(service_path);
  if (!network)
    return NETWORK_NOT_FOUND;

  if (network->connecting_or_connected())
    return CONNECT_NOT_STARTED;

  if (network->type() == TYPE_ETHERNET)
    return CONNECT_NOT_STARTED;  // Normally this shouldn't happen

  if (network->type() == TYPE_WIFI || network->type() == TYPE_VPN) {
    network->SetEnrollmentDelegate(chromeos::CreateEnrollmentDelegate(
        parent_window, network->name(), ProfileManager::GetLastUsedProfile()));
    network->AttemptConnection(base::Bind(&DoConnect, network, parent_window));
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
      ActivateCellular(service_path);
      return CONNECT_STARTED;
    }
    if (cellular->activation_state() == ACTIVATION_STATE_ACTIVATING)
      return CONNECT_NOT_STARTED;
    cros->ConnectToCellularNetwork(cellular);
    return CONNECT_STARTED;
  }

  NOTREACHED();
  return CONNECT_NOT_STARTED;
}

void HandleUnconfiguredNetwork(const std::string& service_path,
                               gfx::NativeWindow parent_window) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  Network* network = cros->FindNetworkByPath(service_path);
  if (!network) {
    LOG(WARNING) << "Unknown network: " << service_path;
    return;
  }

  if (network->type() == TYPE_WIFI || network->type() == TYPE_VPN) {
    network->SetEnrollmentDelegate(chromeos::CreateEnrollmentDelegate(
        parent_window, network->name(), ProfileManager::GetLastUsedProfile()));
    // This will connect to the network only if the network just needs to have
    // its certificate configured. Otherwise it will show an enrollment dialog
    // if available, or call NetworkConfigView::Show().
    network->AttemptConnection(
        base::Bind(&NetworkConfigView::Show, network, parent_window));
    return;
  }

  if (network->type() == TYPE_WIMAX) {
    NetworkConfigView::Show(network, parent_window);
    return;
  }

  if (network->type() == TYPE_CELLULAR) {
    CellularNetwork* cellular = static_cast<CellularNetwork*>(network);
    if (cellular->NeedsActivation()) {
      ActivateCellular(service_path);
      return;
    } else if (cellular->out_of_credits()) {
      ShowMobileSetup(service_path);
      return;
    }
  }

  // No special configure or setup for |service_path|, show the settings UI.
  std::string page = chrome::kInternetOptionsSubPage;
  std::string name = network->name();
  if (name.empty() && network->type() == TYPE_ETHERNET)
    name = l10n_util::GetStringUTF8(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  page += base::StringPrintf(
      "?servicePath=%s&networkType=%d&networkName=%s",
      net::EscapeUrlEncodedData(service_path, true).c_str(),
      network->type(),
      net::EscapeUrlEncodedData(name, false).c_str());
  content::RecordAction(
      content::UserMetricsAction("OpenInternetOptionsDialog"));
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::ShowSettingsSubPage(browser, page);
}

}  // namespace network_connect
}  // namespace chromeos
