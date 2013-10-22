// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"

#include "chromeos/network/network_state.h"

namespace chromeos {

NetworkPortalDetectorTestImpl::NetworkPortalDetectorTestImpl() {}

NetworkPortalDetectorTestImpl::~NetworkPortalDetectorTestImpl() {
}

void NetworkPortalDetectorTestImpl::SetDefaultNetworkPathForTesting(
    const std::string& service_path) {
  if (service_path.empty())
    default_network_.reset();
  else
    default_network_.reset(new NetworkState(service_path));
}

void NetworkPortalDetectorTestImpl::SetDetectionResultsForTesting(
    const std::string& service_path,
    const CaptivePortalState& state) {
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
    const chromeos::NetworkState* network) {
  if (!network || !portal_state_map_.count(network->path()))
    return CaptivePortalState();
  return portal_state_map_[network->path()];
}

bool NetworkPortalDetectorTestImpl::IsEnabled() {
  return true;
}

void NetworkPortalDetectorTestImpl::Enable(bool start_detection) {
}

bool NetworkPortalDetectorTestImpl::StartDetectionIfIdle() {
  return false;
}

void NetworkPortalDetectorTestImpl::EnableLazyDetection() {
}

void NetworkPortalDetectorTestImpl::DisableLazyDetection() {
}

}  // namespace chromeos
