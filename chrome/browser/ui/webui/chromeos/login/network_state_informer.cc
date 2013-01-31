// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "net/proxy/proxy_config.h"

namespace {

// Timeout to smooth temporary network state transitions for flaky networks.
const int kNetworkStateCheckDelayMs = 5000;

}  // namespace

namespace chromeos {

NetworkStateInformer::NetworkStateInformer()
    : state_(OFFLINE),
      delegate_(NULL),
      last_network_type_(TYPE_WIFI) {
}

NetworkStateInformer::~NetworkStateInformer() {
  CrosLibrary::Get()->GetNetworkLibrary()->
      RemoveNetworkManagerObserver(this);
  if (NetworkPortalDetector::IsEnabled() &&
      NetworkPortalDetector::GetInstance()) {
    NetworkPortalDetector::GetInstance()->RemoveObserver(this);
  }
}

void NetworkStateInformer::Init() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  UpdateState(cros);
  cros->AddNetworkManagerObserver(this);

  if (NetworkPortalDetector::IsEnabled() &&
      NetworkPortalDetector::GetInstance()) {
    NetworkPortalDetector::GetInstance()->AddObserver(this);
  }

  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_PROXY_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
}

void NetworkStateInformer::SetDelegate(NetworkStateInformerDelegate* delegate) {
  delegate_ = delegate;
}

void NetworkStateInformer::AddObserver(NetworkStateInformerObserver* observer) {
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkStateInformer::RemoveObserver(
    NetworkStateInformerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkStateInformer::OnNetworkManagerChanged(NetworkLibrary* cros) {
  const Network* active_network = cros->active_network();
  State new_state = OFFLINE;
  std::string new_network_service_path;
  if (active_network) {
    new_state = GetNetworkState(active_network);
    new_network_service_path = active_network->service_path();
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
                   base::Unretained(this)));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        check_state_.callback(),
        base::TimeDelta::FromMilliseconds(kNetworkStateCheckDelayMs));
  }
}

void NetworkStateInformer::OnPortalStateChanged(
    const Network* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  if (CrosLibrary::Get()) {
    NetworkLibrary* network_library =
        CrosLibrary::Get()->GetNetworkLibrary();
    if (network_library && network_library->active_network() == network)
      OnNetworkManagerChanged(network_library);
  }
}

void NetworkStateInformer::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_SESSION_STARTED)
    registrar_.RemoveAll();
  else if (type == chrome::NOTIFICATION_LOGIN_PROXY_CHANGED)
    SendStateToObservers(ErrorScreenHandler::kErrorReasonProxyConfigChanged);
  else
    NOTREACHED() << "Unknown notification: " << type;
}

void NetworkStateInformer::OnPortalDetected() {
  SendStateToObservers(ErrorScreenHandler::kErrorReasonPortalDetected);
}

bool NetworkStateInformer::UpdateState(NetworkLibrary* cros) {
  State new_state = OFFLINE;

  const Network* active_network = cros->active_network();
  if (active_network) {
    new_state = GetNetworkState(active_network);
    last_network_service_path_ = active_network->service_path();
    last_network_type_ = active_network->type();
  }

  bool updated = (new_state != state_) ||
      (new_state != OFFLINE &&
       last_network_service_path_ != last_connected_service_path_);
  state_ = new_state;
  if (state_ != OFFLINE)
    last_connected_service_path_ = last_network_service_path_;

  if (updated && state_ == ONLINE && delegate_)
    delegate_->OnNetworkReady();

  return updated;
}

void NetworkStateInformer::UpdateStateAndNotify() {
  // Cancel pending update request if any.
  check_state_.Cancel();

  if (UpdateState(CrosLibrary::Get()->GetNetworkLibrary()))
    SendStateToObservers(ErrorScreenHandler::kErrorReasonNetworkChanged);
}

void NetworkStateInformer::SendStateToObservers(const std::string& reason) {
  FOR_EACH_OBSERVER(NetworkStateInformerObserver, observers_,
      UpdateState(state_,
                  last_network_service_path_,
                  last_network_type_,
                  reason));
}

NetworkStateInformer::State NetworkStateInformer::GetNetworkState(
    const Network* network) {
  DCHECK(network);
  if (network->online()) {
    // For a proxied network shill's Captive Portal detector isn't
    // activated and we should rely on a Chrome's Captive Portal
    // detector results.
    // For a non-proxied networks to prevent shill's false positives we
    // also rely on a Chrome's Captive Portal detector results.
    if (IsProxyConfigured(network) && ProxyAuthRequired(network))
      return PROXY_AUTH_REQUIRED;
    else if (IsRestrictedPool(network))
      return CAPTIVE_PORTAL;
    else
      return ONLINE;
  } else if (network->connecting()) {
    return CONNECTING;
  } else if (IsRestrictedPool(network)) {
    return CAPTIVE_PORTAL;
  }
  return OFFLINE;
}

bool NetworkStateInformer::IsRestrictedPool(const Network* network) {
  DCHECK(network);
  if (NetworkPortalDetector::IsEnabled()) {
    NetworkPortalDetector::CaptivePortalState state =
        NetworkPortalDetector::GetInstance()->GetCaptivePortalState(network);
    NetworkPortalDetector::CaptivePortalStatus status = state.status;
    return (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  } else {
    return network->restricted_pool();
  }
}

bool NetworkStateInformer::IsProxyConfigured(const Network* network) {
  DCHECK(network);
  ProxyStateMap::iterator it = proxy_state_map_.find(network->unique_id());
  if (it != proxy_state_map_.end() &&
      it->second.proxy_config == network->proxy_config()) {
    return it->second.configured;
  }
  net::ProxyConfig proxy_config;
  if (!ProxyConfigServiceImpl::ParseProxyConfig(network, &proxy_config))
    return false;
  bool configured = !proxy_config.proxy_rules().empty();
  proxy_state_map_[network->unique_id()] =
      ProxyState(network->proxy_config(), configured);
  return configured;
}

bool NetworkStateInformer::ProxyAuthRequired(const Network* network) {
  DCHECK(network);
  if (NetworkPortalDetector::IsEnabled()) {
    NetworkPortalDetector::CaptivePortalState state =
        NetworkPortalDetector::GetInstance()->GetCaptivePortalState(network);
    NetworkPortalDetector::CaptivePortalStatus status = state.status;
    return (status ==
            NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED);
  } else {
    return false;
  }
}

}  // namespace chromeos
