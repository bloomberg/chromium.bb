// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/portal_detector/network_portal_detector.h"

#include "base/logging.h"

namespace chromeos {

namespace {

const char kCaptivePortalStatusUnknown[] = "Unknown";
const char kCaptivePortalStatusOffline[] = "Offline";
const char kCaptivePortalStatusOnline[]  = "Online";
const char kCaptivePortalStatusPortal[]  = "Portal";
const char kCaptivePortalStatusProxyAuthRequired[] = "ProxyAuthRequired";
const char kCaptivePortalStatusUnrecognized[] = "Unrecognized";

}  // namespace

// static
bool NetworkPortalDetector::set_for_testing_ = false;
NetworkPortalDetector* NetworkPortalDetector::network_portal_detector_ = NULL;

// static
void NetworkPortalDetector::InitializeForTesting(
    NetworkPortalDetector* network_portal_detector) {
  if (network_portal_detector) {
    CHECK(!set_for_testing_)
        << "NetworkPortalDetector::InitializeForTesting is called twice";
    CHECK(network_portal_detector);
    delete network_portal_detector_;
    network_portal_detector_ = network_portal_detector;
    set_for_testing_ = true;
  } else {
    network_portal_detector_ = NULL;
    set_for_testing_ = false;
  }
}

// static
bool NetworkPortalDetector::IsInitialized() {
  return NetworkPortalDetector::network_portal_detector_;
}

// static
void NetworkPortalDetector::Shutdown() {
  CHECK(network_portal_detector_ || set_for_testing_)
      << "NetworkPortalDetector::Shutdown() called without Initialize()";
  delete network_portal_detector_;
  network_portal_detector_ = NULL;
}

// static
NetworkPortalDetector* NetworkPortalDetector::Get() {
  CHECK(network_portal_detector_)
      << "NetworkPortalDetector::Get() called before Initialize()";
  return network_portal_detector_;
}

// static
std::string NetworkPortalDetector::CaptivePortalStatusString(
    CaptivePortalStatus status) {
  switch (status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
      return kCaptivePortalStatusUnknown;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      return kCaptivePortalStatusOffline;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      return kCaptivePortalStatusOnline;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      return kCaptivePortalStatusPortal;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      return kCaptivePortalStatusProxyAuthRequired;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT:
      NOTREACHED();
  }
  return kCaptivePortalStatusUnrecognized;
}

// NetworkPortalDetectorStubImpl

NetworkPortalDetectorStubImpl::NetworkPortalDetectorStubImpl() {
}

NetworkPortalDetectorStubImpl::~NetworkPortalDetectorStubImpl() {
}

void NetworkPortalDetectorStubImpl::AddObserver(Observer* /* observer */) {
}

void NetworkPortalDetectorStubImpl::AddAndFireObserver(Observer* observer) {
  if (observer)
    observer->OnPortalDetectionCompleted(NULL, CaptivePortalState());
}

void NetworkPortalDetectorStubImpl::RemoveObserver(Observer* /* observer */) {
}

NetworkPortalDetector::CaptivePortalState
NetworkPortalDetectorStubImpl::GetCaptivePortalState(
    const std::string& /* service_path */) {
  return CaptivePortalState();
}

bool NetworkPortalDetectorStubImpl::IsEnabled() {
  return false;
}

void NetworkPortalDetectorStubImpl::Enable(bool /* start_detection */) {
}

bool NetworkPortalDetectorStubImpl::StartDetectionIfIdle() {
  return false;
}

void NetworkPortalDetectorStubImpl::SetStrategy(
    PortalDetectorStrategy::StrategyId /* id */) {
}

}  // namespace chromeos
