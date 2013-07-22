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

namespace {

// Timeout to smooth temporary network state transitions for flaky networks.
const int kNetworkStateCheckDelaySec = 3;

}  // namespace

namespace chromeos {

NetworkStateInformer::NetworkStateInformer()
    : state_(OFFLINE),
      weak_ptr_factory_(this) {
}

NetworkStateInformer::~NetworkStateInformer() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
  if (NetworkPortalDetector::IsEnabledInCommandLine() &&
      NetworkPortalDetector::GetInstance()) {
    NetworkPortalDetector::GetInstance()->RemoveObserver(this);
  }
}

void NetworkStateInformer::Init() {
  UpdateState();
  NetworkHandler::Get()->network_state_handler()->AddObserver(
      this, FROM_HERE);

  if (NetworkPortalDetector::IsEnabledInCommandLine() &&
      NetworkPortalDetector::GetInstance()) {
    NetworkPortalDetector::GetInstance()->AddAndFireObserver(this);
  }

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

void NetworkStateInformer::NetworkManagerChanged() {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  State new_state = OFFLINE;
  std::string new_network_service_path;
  if (default_network) {
    new_state = GetNetworkState(default_network);
    new_network_service_path = default_network->path();
  }
  if ((state_ != ONLINE && (new_state == ONLINE || new_state == CONNECTING)) ||
      (state_ == ONLINE && (new_state == ONLINE || new_state == CONNECTING) &&
       new_network_service_path != last_online_service_path_) ||
      (new_state == CAPTIVE_PORTAL &&
       new_network_service_path == last_network_service_path_)) {
    last_network_service_path_ = new_network_service_path;
    if (new_state == ONLINE)
      last_online_service_path_ = new_network_service_path;
    // Transitions {OFFLINE, PORTAL} -> ONLINE and connections to
    // different network are processed without delay.
    // Transitions {OFFLINE, ONLINE} -> PORTAL in the same network are
    // also processed without delay.
    UpdateStateAndNotify();
  } else {
    check_state_.Cancel();
    check_state_.Reset(
        base::Bind(&NetworkStateInformer::UpdateStateAndNotify,
                   weak_ptr_factory_.GetWeakPtr()));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        check_state_.callback(),
        base::TimeDelta::FromSeconds(kNetworkStateCheckDelaySec));
  }
}

void NetworkStateInformer::DefaultNetworkChanged(const NetworkState* network) {
  NetworkManagerChanged();
}

void NetworkStateInformer::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  if (NetworkHandler::IsInitialized() &&
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork() ==
      network)
    NetworkManagerChanged();
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
  SendStateToObservers(ErrorScreenActor::ERROR_REASON_PORTAL_DETECTED);
}

bool NetworkStateInformer::UpdateState() {
  State new_state = OFFLINE;

  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (default_network) {
    new_state = GetNetworkState(default_network);
    last_network_service_path_ = default_network->path();
    last_network_type_ = default_network->type();
  }

  bool updated = (new_state != state_) ||
      (new_state != OFFLINE &&
       last_network_service_path_ != last_connected_service_path_);
  state_ = new_state;
  if (state_ != OFFLINE)
    last_connected_service_path_ = last_network_service_path_;

  if (updated && state_ == ONLINE) {
    FOR_EACH_OBSERVER(NetworkStateInformerObserver, observers_,
                      OnNetworkReady());
  }

  return updated;
}

void NetworkStateInformer::UpdateStateAndNotify() {
  // Cancel pending update request if any.
  check_state_.Cancel();

  if (UpdateState())
    SendStateToObservers(ErrorScreenActor::ERROR_REASON_NETWORK_STATE_CHANGED);
  else
    SendStateToObservers(ErrorScreenActor::ERROR_REASON_UPDATE);
}

void NetworkStateInformer::SendStateToObservers(
    ErrorScreenActor::ErrorReason reason) {
  FOR_EACH_OBSERVER(NetworkStateInformerObserver, observers_,
      UpdateState(state_, reason));
}

NetworkStateInformer::State NetworkStateInformer::GetNetworkState(
    const NetworkState* network) {
  DCHECK(network);
  if (NetworkPortalDetector::IsEnabledInCommandLine() &&
      NetworkPortalDetector::GetInstance()) {
    NetworkPortalDetector::CaptivePortalState state =
        NetworkPortalDetector::GetInstance()->GetCaptivePortalState(network);
    NetworkPortalDetector::CaptivePortalStatus status = state.status;
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN &&
        NetworkState::StateIsConnecting(network->connection_state())) {
      return CONNECTING;
    }
    // For proxy-less networks rely on shill's online state if
    // NetworkPortalDetector's state of current network is unknown.
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE ||
        (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN &&
         !IsProxyConfigured(network) &&
         network->connection_state() == flimflam::kStateOnline)) {
      return ONLINE;
    }
    if (status ==
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED &&
        IsProxyConfigured(network)) {
      return PROXY_AUTH_REQUIRED;
    }
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL ||
        (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN &&
         network->connection_state() == flimflam::kStatePortal))
      return CAPTIVE_PORTAL;
  } else {
    if (NetworkState::StateIsConnecting(network->connection_state()))
      return CONNECTING;
    if (network->connection_state() == flimflam::kStateOnline)
      return ONLINE;
    if (network->connection_state() == flimflam::kStatePortal)
      return CAPTIVE_PORTAL;
  }
  return OFFLINE;
}

bool NetworkStateInformer::IsProxyConfigured(const NetworkState* network) {
  DCHECK(network);
  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  scoped_ptr<ProxyConfigDictionary> proxy_dict =
      proxy_config::GetProxyConfigForNetwork(
          NULL, g_browser_process->local_state(), *network, &onc_source);
  ProxyPrefs::ProxyMode mode;
  return (proxy_dict &&
          proxy_dict->GetMode(&mode) &&
          mode == ProxyPrefs::MODE_FIXED_SERVERS);
}

}  // namespace chromeos
