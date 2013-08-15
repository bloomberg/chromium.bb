// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_connect.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/chromeos/network/network_state_notifier.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::DeviceState;
using chromeos::NetworkConfigurationHandler;
using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkProfile;
using chromeos::NetworkProfileHandler;
using chromeos::NetworkState;

namespace ash {

namespace {

// TODO(stevenjb): This should be in service_constants.h
const char kErrorInProgress[] = "org.chromium.flimflam.Error.InProgress";

// Returns true for carriers that can be activated through Shill instead of
// through a WebUI dialog.
bool IsDirectActivatedCarrier(const std::string& carrier) {
  if (carrier == shill::kCarrierSprint)
    return true;
  return false;
}

void ShowErrorNotification(const std::string& error,
                           const std::string& service_path) {
  Shell::GetInstance()->system_tray_notifier()->network_state_notifier()->
      ShowNetworkConnectError(error, service_path);
}

void OnConnectFailed(const std::string& service_path,
                     gfx::NativeWindow owning_window,
                     const std::string& error_name,
                     scoped_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR("Connect Failed: " + error_name, service_path);

  // If a new connect attempt canceled this connect, no need to notify the user.
  if (error_name == NetworkConnectionHandler::kErrorConnectCanceled)
    return;

  if (error_name == NetworkConnectionHandler::kErrorPassphraseRequired ||
      error_name == NetworkConnectionHandler::kErrorConfigurationRequired ||
      error_name == NetworkConnectionHandler::kErrorAuthenticationRequired) {
    ash::Shell::GetInstance()->system_tray_delegate()->ConfigureNetwork(
        service_path);
    return;
  }

  if (error_name == NetworkConnectionHandler::kErrorCertificateRequired) {
    ash::Shell::GetInstance()->system_tray_delegate()->EnrollOrConfigureNetwork(
        service_path, owning_window);
    return;
  }

  if (error_name == NetworkConnectionHandler::kErrorActivationRequired) {
    network_connect::ActivateCellular(service_path);
    return;
  }

  if (error_name == NetworkConnectionHandler::kErrorConnected ||
      error_name == NetworkConnectionHandler::kErrorConnecting) {
    ash::Shell::GetInstance()->system_tray_delegate()->ShowNetworkSettings(
        service_path);
    return;
  }

  // ConnectFailed or unknown error; show a notification.
  ShowErrorNotification(error_name, service_path);

  // Show a configure dialog for ConnectFailed errors.
  if (error_name != NetworkConnectionHandler::kErrorConnectFailed)
    return;

  // If Shill reports an InProgress error, don't try to configure the network.
  std::string dbus_error_name;
  error_data.get()->GetString(
      chromeos::network_handler::kDbusErrorName, &dbus_error_name);
  if (dbus_error_name == kErrorInProgress)
    return;

  ash::Shell::GetInstance()->system_tray_delegate()->ConfigureNetwork(
      service_path);
}

void OnConnectSucceeded(const std::string& service_path) {
  NET_LOG_USER("Connect Succeeded", service_path);
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyClearNetworkMessage(
      NetworkObserver::ERROR_CONNECT_FAILED);
}

// If |check_error_state| is true, error state for the network is checked,
// otherwise any current error state is ignored (e.g. for recently configured
// networks or repeat connect attempts). |owning_window| will be used to parent
// any configuration UI on failure and may be NULL (in which case the default
// window will be used).
void CallConnectToNetwork(const std::string& service_path,
                          bool check_error_state,
                          gfx::NativeWindow owning_window) {
  NET_LOG_USER("ConnectToNetwork", service_path);

  ash::Shell::GetInstance()->system_tray_notifier()->NotifyClearNetworkMessage(
      NetworkObserver::ERROR_CONNECT_FAILED);

  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path,
      base::Bind(&OnConnectSucceeded, service_path),
      base::Bind(&OnConnectFailed, service_path, owning_window),
      check_error_state);
}

void OnActivateFailed(const std::string& service_path,
                     const std::string& error_name,
                      scoped_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR("Unable to activate network", service_path);
  ShowErrorNotification(
      NetworkConnectionHandler::kErrorActivateFailed, service_path);
}

void OnActivateSucceeded(const std::string& service_path) {
  NET_LOG_USER("Activation Succeeded", service_path);
}

void OnConfigureFailed(const std::string& error_name,
                       scoped_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR("Unable to configure network", "");
  ShowErrorNotification(NetworkConnectionHandler::kErrorConfigureFailed, "");
}

void OnConfigureSucceeded(const std::string& service_path) {
  NET_LOG_USER("Configure Succeeded", service_path);
  // After configuring a network, ignore any (possibly stale) error state.
  const bool check_error_state = false;
  const gfx::NativeWindow owning_window = NULL;
  CallConnectToNetwork(service_path, check_error_state, owning_window);
}

void SetPropertiesFailed(const std::string& desc,
                         const std::string& service_path,
                         const std::string& config_error_name,
                         scoped_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR(desc + ": Failed: " + config_error_name, service_path);
  ShowErrorNotification(
      NetworkConnectionHandler::kErrorConfigureFailed, service_path);
}

void SetPropertiesToClear(base::DictionaryValue* properties_to_set,
                          std::vector<std::string>* properties_to_clear) {
  // Move empty string properties to properties_to_clear.
  for (base::DictionaryValue::Iterator iter(*properties_to_set);
       !iter.IsAtEnd(); iter.Advance()) {
    std::string value_str;
    if (iter.value().GetAsString(&value_str) && value_str.empty())
      properties_to_clear->push_back(iter.key());
  }
  // Remove cleared properties from properties_to_set.
  for (std::vector<std::string>::iterator iter = properties_to_clear->begin();
       iter != properties_to_clear->end(); ++iter) {
    properties_to_set->RemoveWithoutPathExpansion(*iter, NULL);
  }
}

void ClearPropertiesAndConnect(
    const std::string& service_path,
    const std::vector<std::string>& properties_to_clear) {
  NET_LOG_USER("ClearPropertiesAndConnect", service_path);
  // After configuring a network, ignore any (possibly stale) error state.
  const bool check_error_state = false;
  const gfx::NativeWindow owning_window = NULL;
  NetworkHandler::Get()->network_configuration_handler()->ClearProperties(
      service_path,
      properties_to_clear,
      base::Bind(&CallConnectToNetwork,
                 service_path, check_error_state,
                 owning_window),
      base::Bind(&SetPropertiesFailed, "ClearProperties", service_path));
}

// Returns false if !shared and no valid profile is available, which will
// trigger an error and abort.
bool GetNetworkProfilePath(bool shared, std::string* profile_path) {
  if (shared) {
    *profile_path = NetworkProfileHandler::kSharedProfilePath;
    return true;
  }

  if (!chromeos::LoginState::Get()->IsUserAuthenticated()) {
    NET_LOG_ERROR("User profile specified before login", "");
    return false;
  }

  const NetworkProfile* profile  =
      NetworkHandler::Get()->network_profile_handler()->
      GetDefaultUserProfile();
  if (!profile) {
    NET_LOG_ERROR("No user profile for unshared network configuration", "");
    return false;
  }

  *profile_path = profile->path;
  return true;
}

void ConfigureSetProfileSucceeded(
    const std::string& service_path,
    scoped_ptr<base::DictionaryValue> properties_to_set) {
  std::vector<std::string> properties_to_clear;
  SetPropertiesToClear(properties_to_set.get(), &properties_to_clear);
  NetworkHandler::Get()->network_configuration_handler()->SetProperties(
      service_path,
      *properties_to_set,
      base::Bind(&ClearPropertiesAndConnect,
                 service_path,
                 properties_to_clear),
      base::Bind(&SetPropertiesFailed, "SetProperties", service_path));
}

}  // namespace

namespace network_connect {

void ConnectToNetwork(const std::string& service_path,
                      gfx::NativeWindow owning_window) {
  const bool check_error_state = true;
  CallConnectToNetwork(service_path, check_error_state, owning_window);
}

void ActivateCellular(const std::string& service_path) {
  NET_LOG_USER("ActivateCellular", service_path);
  const NetworkState* cellular =
      NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (!cellular || cellular->type() != flimflam::kTypeCellular) {
    NET_LOG_ERROR("ActivateCellular with no Service", service_path);
    return;
  }
  const DeviceState* cellular_device =
      NetworkHandler::Get()->network_state_handler()->
      GetDeviceState(cellular->device_path());
  if (!cellular_device) {
    NET_LOG_ERROR("ActivateCellular with no Device", service_path);
    return;
  }
  if (!IsDirectActivatedCarrier(cellular_device->carrier())) {
    // For non direct activation, show the mobile setup dialog which can be
    // used to activate the network. Only show the dialog, if an account
    // management URL is available.
    if (!cellular->payment_url().empty())
      ash::Shell::GetInstance()->system_tray_delegate()->ShowMobileSetup(
          service_path);
    return;
  }
  if (cellular->activation_state() == flimflam::kActivationStateActivated) {
    NET_LOG_ERROR("ActivateCellular for activated service", service_path);
    return;
  }

  NetworkHandler::Get()->network_connection_handler()->ActivateNetwork(
      service_path,
      "",  // carrier
      base::Bind(&OnActivateSucceeded, service_path),
      base::Bind(&OnActivateFailed, service_path));
}

void ConfigureNetworkAndConnect(const std::string& service_path,
                                const base::DictionaryValue& properties,
                                bool shared) {
  NET_LOG_USER("ConfigureNetworkAndConnect", service_path);

  scoped_ptr<base::DictionaryValue> properties_to_set(properties.DeepCopy());

  std::string profile_path;
  if (!GetNetworkProfilePath(shared, &profile_path)) {
    ShowErrorNotification(
        NetworkConnectionHandler::kErrorConfigureFailed, service_path);
    return;
  }
  NetworkHandler::Get()->network_configuration_handler()->SetNetworkProfile(
      service_path, profile_path,
      base::Bind(&ConfigureSetProfileSucceeded,
                 service_path, base::Passed(&properties_to_set)),
      base::Bind(&SetPropertiesFailed,
                 "SetProfile: " + profile_path, service_path));
}

void CreateConfigurationAndConnect(base::DictionaryValue* properties,
                                   bool shared) {
  NET_LOG_USER("CreateConfigurationAndConnect", "");
  std::string profile_path;
  if (!GetNetworkProfilePath(shared, &profile_path)) {
    ShowErrorNotification(NetworkConnectionHandler::kErrorConfigureFailed, "");
    return;
  }
  properties->SetStringWithoutPathExpansion(
      flimflam::kProfileProperty, profile_path);
  NetworkHandler::Get()->network_configuration_handler()->CreateConfiguration(
      *properties,
      base::Bind(&OnConfigureSucceeded),
      base::Bind(&OnConfigureFailed));
}

string16 ErrorString(const std::string& error) {
  if (error.empty())
    return string16();
  if (error == flimflam::kErrorOutOfRange)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OUT_OF_RANGE);
  if (error == flimflam::kErrorPinMissing)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_PIN_MISSING);
  if (error == flimflam::kErrorDhcpFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_DHCP_FAILED);
  if (error == flimflam::kErrorConnectFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
  if (error == flimflam::kErrorBadPassphrase)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_PASSPHRASE);
  if (error == flimflam::kErrorBadWEPKey)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_WEPKEY);
  if (error == flimflam::kErrorActivationFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
  }
  if (error == flimflam::kErrorNeedEvdo)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_NEED_EVDO);
  if (error == flimflam::kErrorNeedHomeNetwork) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_NEED_HOME_NETWORK);
  }
  if (error == flimflam::kErrorOtaspFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OTASP_FAILED);
  if (error == flimflam::kErrorAaaFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_AAA_FAILED);
  if (error == flimflam::kErrorInternal)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_INTERNAL);
  if (error == flimflam::kErrorDNSLookupFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_DNS_LOOKUP_FAILED);
  }
  if (error == flimflam::kErrorHTTPGetFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_HTTP_GET_FAILED);
  }
  if (error == flimflam::kErrorIpsecPskAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_IPSEC_PSK_AUTH_FAILED);
  }
  if (error == flimflam::kErrorIpsecCertAuthFailed ||
      error == shill::kErrorEapAuthenticationFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CERT_AUTH_FAILED);
  }
  if (error == shill::kErrorEapLocalTlsFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_EAP_LOCAL_TLS_FAILED);
  }
  if (error == shill::kErrorEapRemoteTlsFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_EAP_REMOTE_TLS_FAILED);
  }
  if (error == flimflam::kErrorPppAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_PPP_AUTH_FAILED);
  }

  if (StringToLowerASCII(error) ==
      StringToLowerASCII(std::string(flimflam::kUnknownString))) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
  }
  return l10n_util::GetStringFUTF16(IDS_NETWORK_UNRECOGNIZED_ERROR,
                                    UTF8ToUTF16(error));
}

}  // network_connect
}  // ash
