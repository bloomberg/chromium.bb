// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"

#include "chromeos/network/network_state.h"

namespace chromeos {

NetworkPortalDetectorTestImpl::NetworkPortalDetectorTestImpl()
    : strategy_id_(PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN) {
}

NetworkPortalDetectorTestImpl::~NetworkPortalDetectorTestImpl() {
}

void NetworkPortalDetectorTestImpl::SetDefaultNetworkPathForTesting(
    const std::string& service_path,
    const std::string& guid) {
  DVLOG(1) << "SetDefaultNetworkPathForTesting:"
           << " service path: " << service_path
           << " guid: " << guid;
  if (service_path.empty()) {
    default_network_.reset();
  } else {
    default_network_.reset(new NetworkState(service_path));
    default_network_->SetGuid(guid);
  }
}

void NetworkPortalDetectorTestImpl::SetDetectionResultsForTesting(
    const std::string& service_path,
    const CaptivePortalState& state) {
  DVLOG(1) << "SetDetectionResultsForTesting: " << service_path << " = "
           << NetworkPortalDetector::CaptivePortalStatusString(state.status);
  if (!service_path.empty())
    portal_state_map_[service_path] = state;
}

void NetworkPortalDetectorTestImpl::NotifyObserversForTesting() {
  CaptivePortalState state;
  if (default_network_ &&
      portal_state_map_.count(default_network_->path())) {
    state = portal_state_map_[default_network_->path()];
  }
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnPortalDetectionCompleted(default_network_.get(), state));
}

void NetworkPortalDetectorTestImpl::AddObserver(Observer* observer) {
  if (observer && !observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkPortalDetectorTestImpl::AddAndFireObserver(Observer* observer) {
  AddObserver(observer);
  if (!observer)
    return;
  if (!default_network_ ||
      !portal_state_map_.count(default_network_->path())) {
    observer->OnPortalDetectionCompleted(default_network_.get(),
                                         CaptivePortalState());
  } else {
    observer->OnPortalDetectionCompleted(
        default_network_.get(),
        portal_state_map_[default_network_->path()]);
  }
}

void NetworkPortalDetectorTestImpl::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

NetworkPortalDetector::CaptivePortalState
NetworkPortalDetectorTestImpl::GetCaptivePortalState(
    const std::string& service_path) {
  CaptivePortalStateMap::iterator it = portal_state_map_.find(service_path);
  if (it == portal_state_map_.end()) {
    DVLOG(2) << "GetCaptivePortalState Not found: " << service_path;
    return CaptivePortalState();
  }
  DVLOG(2) << "GetCaptivePortalState: " << service_path << " = "
           << CaptivePortalStatusString(it->second.status);
  return it->second;
}

bool NetworkPortalDetectorTestImpl::IsEnabled() {
  return true;
}

void NetworkPortalDetectorTestImpl::Enable(bool start_detection) {
}

bool NetworkPortalDetectorTestImpl::StartDetectionIfIdle() {
  return false;
}

void NetworkPortalDetectorTestImpl::SetStrategy(
    PortalDetectorStrategy::StrategyId id) {
  strategy_id_ = id;
}

}  // namespace chromeos
