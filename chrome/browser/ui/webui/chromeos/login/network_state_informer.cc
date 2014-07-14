// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/net/proxy_config_handler.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "net/proxy/proxy_config.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kNetworkStateOffline[] = "offline";
const char kNetworkStateOnline[] = "online";
const char kNetworkStateCaptivePortal[] = "behind captive portal";
const char kNetworkStateConnecting[] = "connecting";
const char kNetworkStateProxyAuthRequired[] = "proxy auth required";

bool HasDefaultNetworkProxyConfigured() {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!network)
    return false;
  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  scoped_ptr<ProxyConfigDictionary> proxy_dict =
      proxy_config::GetProxyConfigForNetwork(
          NULL, g_browser_process->local_state(), *network, &onc_source);
  ProxyPrefs::ProxyMode mode;
  return (proxy_dict && proxy_dict->GetMode(&mode) &&
          mode == ProxyPrefs::MODE_FIXED_SERVERS);
}

NetworkStateInformer::State GetStateForDefaultNetwork() {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!network)
    return NetworkStateInformer::OFFLINE;

  if (NetworkPortalDetector::Get()->IsEnabled()) {
    NetworkPortalDetector::CaptivePortalState state =
        NetworkPortalDetector::Get()->GetCaptivePortalState(network->guid());
    NetworkPortalDetector::CaptivePortalStatus status = state.status;
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN &&
        NetworkState::StateIsConnecting(network->connection_state())) {
      return NetworkStateInformer::CONNECTING;
    }
    // For proxy-less networks rely on shill's online state if
    // NetworkPortalDetector's state of current network is unknown.
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE ||
        (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN &&
         !HasDefaultNetworkProxyConfigured() &&
         network->connection_state() == shill::kStateOnline)) {
      return NetworkStateInformer::ONLINE;
    }
    if (status ==
            NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED &&
        HasDefaultNetworkProxyConfigured()) {
      return NetworkStateInformer::PROXY_AUTH_REQUIRED;
    }
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL ||
        (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN &&
         network->connection_state() == shill::kStatePortal))
      return NetworkStateInformer::CAPTIVE_PORTAL;
  } else {
    if (NetworkState::StateIsConnecting(network->connection_state()))
      return NetworkStateInformer::CONNECTING;
    if (network->connection_state() == shill::kStateOnline)
      return NetworkStateInformer::ONLINE;
    if (network->connection_state() == shill::kStatePortal)
      return NetworkStateInformer::CAPTIVE_PORTAL;
  }
  return NetworkStateInformer::OFFLINE;
}

}  // namespace

NetworkStateInformer::NetworkStateInformer()
    : state_(OFFLINE),
      weak_ptr_factory_(this) {
}

NetworkStateInformer::~NetworkStateInformer() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
  NetworkPortalDetector::Get()->RemoveObserver(this);
}

void NetworkStateInformer::Init() {
  UpdateState();
  NetworkHandler::Get()->network_state_handler()->AddObserver(
      this, FROM_HERE);

  NetworkPortalDetector::Get()->AddAndFireObserver(this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_PROXY_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
}

void NetworkStateInformer::AddObserver(NetworkStateInformerObserver* observer) {
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkStateInformer::RemoveObserver(
    NetworkStateInformerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkStateInformer::DefaultNetworkChanged(const NetworkState* network) {
  UpdateStateAndNotify();
}

void NetworkStateInformer::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  UpdateStateAndNotify();
}

void NetworkStateInformer::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_SESSION_STARTED)
    registrar_.RemoveAll();
  else if (type == chrome::NOTIFICATION_LOGIN_PROXY_CHANGED)
    SendStateToObservers(ErrorScreenActor::ERROR_REASON_PROXY_CONFIG_CHANGED);
  else
    NOTREACHED() << "Unknown notification: " << type;
}

void NetworkStateInformer::OnPortalDetected() {
  UpdateStateAndNotify();
}

// static
const char* NetworkStateInformer::StatusString(State state) {
  switch (state) {
    case OFFLINE:
      return kNetworkStateOffline;
    case ONLINE:
      return kNetworkStateOnline;
    case CAPTIVE_PORTAL:
      return kNetworkStateCaptivePortal;
    case CONNECTING:
      return kNetworkStateConnecting;
    case PROXY_AUTH_REQUIRED:
      return kNetworkStateProxyAuthRequired;
    default:
      NOTREACHED();
      return NULL;
  }
}

bool NetworkStateInformer::UpdateState() {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  State new_state = GetStateForDefaultNetwork();
  std::string new_network_path;
  std::string new_network_type;
  if (default_network) {
    new_network_path = default_network->path();
    new_network_type = default_network->type();
  }

  bool updated = (new_state != state_) ||
      (new_network_path != network_path_) ||
      (new_network_type != network_type_);
  state_ = new_state;
  network_path_ = new_network_path;
  network_type_ = new_network_type;

  if (updated && state_ == ONLINE) {
    FOR_EACH_OBSERVER(NetworkStateInformerObserver, observers_,
                      OnNetworkReady());
  }

  return updated;
}

void NetworkStateInformer::UpdateStateAndNotify() {
  if (UpdateState())
    SendStateToObservers(ErrorScreenActor::ERROR_REASON_NETWORK_STATE_CHANGED);
  else
    SendStateToObservers(ErrorScreenActor::ERROR_REASON_UPDATE);
}

void NetworkStateInformer::SendStateToObservers(
    ErrorScreenActor::ErrorReason reason) {
  FOR_EACH_OBSERVER(NetworkStateInformerObserver, observers_,
      UpdateState(reason));
}

}  // namespace chromeos
