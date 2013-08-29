// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector_stub.h"

#include "chromeos/network/network_state.h"

namespace chromeos {

NetworkPortalDetectorStub::NetworkPortalDetectorStub() {}

NetworkPortalDetectorStub::~NetworkPortalDetectorStub() {
}

void NetworkPortalDetectorStub::SetDefaultNetworkPathForTesting(
    const std::string& service_path) {
  if (service_path.empty())
    default_network_.reset();
  else
    default_network_.reset(new NetworkState(service_path));
}

void NetworkPortalDetectorStub::SetDetectionResultsForTesting(
    const std::string& service_path,
    const CaptivePortalState& state) {
  if (!service_path.empty())
    portal_state_map_[service_path] = state;
}

void NetworkPortalDetectorStub::NotifyObserversForTesting() {
  CaptivePortalState state;
  if (default_network_ &&
      portal_state_map_.count(default_network_->path())) {
    state = portal_state_map_[default_network_->path()];
  }
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnPortalDetectionCompleted(default_network_.get(), state));
}

void NetworkPortalDetectorStub::Init() {
}

void NetworkPortalDetectorStub::Shutdown() {
}

void NetworkPortalDetectorStub::AddObserver(Observer* observer) {
  if (observer && !observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkPortalDetectorStub::AddAndFireObserver(Observer* observer) {
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

void NetworkPortalDetectorStub::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

NetworkPortalDetector::CaptivePortalState
NetworkPortalDetectorStub::GetCaptivePortalState(
    const chromeos::NetworkState* network) {
  if (!network || !portal_state_map_.count(network->path()))
    return CaptivePortalState();
  return portal_state_map_[network->path()];
}

bool NetworkPortalDetectorStub::IsEnabled() {
  return true;
}

void NetworkPortalDetectorStub::Enable(bool start_detection) {
}

bool NetworkPortalDetectorStub::StartDetectionIfIdle() {
  return false;
}

void NetworkPortalDetectorStub::EnableLazyDetection() {
}

void NetworkPortalDetectorStub::DisableLazyDetection() {
}

}  // namespace chromeos
