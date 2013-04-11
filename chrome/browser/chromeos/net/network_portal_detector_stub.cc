// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector_stub.h"

#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

NetworkPortalDetectorStub::NetworkPortalDetectorStub() : active_network_(NULL) {
}

NetworkPortalDetectorStub::~NetworkPortalDetectorStub() {
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
  if (!active_network_ ||
      !portal_state_map_.count(active_network_->service_path())) {
    observer->OnPortalDetectionCompleted(active_network_, CaptivePortalState());
  } else {
    observer->OnPortalDetectionCompleted(
        active_network_,
        portal_state_map_[active_network_->service_path()]);
  }
}

void NetworkPortalDetectorStub::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

NetworkPortalDetector::CaptivePortalState
NetworkPortalDetectorStub::GetCaptivePortalState(
    const chromeos::Network* network) {
  if (!network || !portal_state_map_.count(network->service_path()))
    return CaptivePortalState();
  return portal_state_map_[network->service_path()];
}

bool NetworkPortalDetectorStub::IsEnabled() {
  return true;
}

void NetworkPortalDetectorStub::Enable(bool start_detection) {
}

void NetworkPortalDetectorStub::EnableLazyDetection() {
}

void NetworkPortalDetectorStub::DisableLazyDetection() {
}

void NetworkPortalDetectorStub::SetActiveNetworkForTesting(
    const Network* network) {
  active_network_ = network;
}

void NetworkPortalDetectorStub::SetDetectionResultsForTesting(
    const Network* network,
    const CaptivePortalState& state) {
  if (!network)
    return;
  portal_state_map_[network->service_path()] = state;
}

void NetworkPortalDetectorStub::NotifyObserversForTesting() {
  CaptivePortalState state;
  if (active_network_ &&
      portal_state_map_.count(active_network_->service_path())) {
    state = portal_state_map_[active_network_->service_path()];
  }
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnPortalDetectionCompleted(active_network_, state));
}

}  // namespace chromeos
