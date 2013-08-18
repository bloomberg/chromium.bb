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
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/certificate_pattern.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

void EnrollmentComplete(const std::string& service_path) {
  NET_LOG_USER("Enrollment Complete", service_path);
}

}

namespace network_connect {

void ShowMobileSetup(const std::string& service_path) {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const NetworkState* cellular = handler->GetNetworkState(service_path);
  if (cellular && cellular->type() == flimflam::kTypeCellular &&
      cellular->activation_state() != flimflam::kActivationStateActivated &&
      cellular->activate_over_non_cellular_networks() &&
      !handler->DefaultNetwork()) {
    std::string technology = cellular->network_technology();
    ash::NetworkObserver::NetworkType network_type =
        (technology == flimflam::kNetworkTechnologyLte ||
         technology == flimflam::kNetworkTechnologyLteAdvanced)
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

void ShowNetworkSettings(const std::string& service_path) {
  std::string page = chrome::kInternetOptionsSubPage;
  const NetworkState* network = service_path.empty() ? NULL :
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (network) {
    std::string name(network->name());
    if (name.empty() && network->type() == flimflam::kTypeEthernet)
      name = l10n_util::GetStringUTF8(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    page += base::StringPrintf(
        "?servicePath=%s&networkType=%s&networkName=%s",
        net::EscapeUrlEncodedData(service_path, true).c_str(),
        net::EscapeUrlEncodedData(network->type(), true).c_str(),
        net::EscapeUrlEncodedData(name, false).c_str());
  }
  content::RecordAction(
      content::UserMetricsAction("OpenInternetOptionsDialog"));
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::ShowSettingsSubPage(browser, page);
}

void HandleUnconfiguredNetwork(const std::string& service_path,
                               gfx::NativeWindow parent_window) {
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (!network) {
    NET_LOG_ERROR("Configuring unknown network", service_path);
    return;
  }

  if (network->type() == flimflam::kTypeWifi) {
    // Only show the config view for secure networks, otherwise do nothing.
    if (network->security() != flimflam::kSecurityNone)
      NetworkConfigView::Show(service_path, parent_window);
    return;
  }

  if (network->type() == flimflam::kTypeWimax ||
      network->type() == flimflam::kTypeVPN) {
    NetworkConfigView::Show(service_path, parent_window);
    return;
  }

  if (network->type() == flimflam::kTypeCellular) {
    if (network->activation_state() != flimflam::kActivationStateActivated) {
      ash::network_connect::ActivateCellular(service_path);
      return;
    }
    if (network->cellular_out_of_credits()) {
      ShowMobileSetup(service_path);
      return;
    }
    // No special configure or setup for |network|, show the settings UI.
    ShowNetworkSettings(service_path);
  }
  NOTREACHED();
}

bool EnrollNetwork(const std::string& service_path,
                   gfx::NativeWindow parent_window) {
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (!network) {
    NET_LOG_ERROR("Enrolling Unknown network", service_path);
    return false;
  }
  // We skip certificate patterns for device policy ONC so that an unmanaged
  // user can't get to the place where a cert is presented for them
  // involuntarily.
  if (network->ui_data().onc_source() == onc::ONC_SOURCE_DEVICE_POLICY)
    return false;

  const CertificatePattern& certificate_pattern =
      network->ui_data().certificate_pattern();
  if (certificate_pattern.Empty())
    return false;

  NET_LOG_USER("Enrolling", service_path);

  EnrollmentDelegate* enrollment = CreateEnrollmentDelegate(
      parent_window, network->name(), ProfileManager::GetDefaultProfile());
  return enrollment->Enroll(certificate_pattern.enrollment_uri_list(),
                            base::Bind(&EnrollmentComplete, service_path));
}

const base::DictionaryValue* FindPolicyForActiveUser(
    const NetworkState* network,
    onc::ONCSource* onc_source) {
  const User* user = UserManager::Get()->GetActiveUser();
  std::string username_hash = user ? user->username_hash() : std::string();
  return NetworkHandler::Get()->managed_network_configuration_handler()->
      FindPolicyByGUID(username_hash, network->guid(), onc_source);
}

}  // namespace network_connect
}  // namespace chromeos
