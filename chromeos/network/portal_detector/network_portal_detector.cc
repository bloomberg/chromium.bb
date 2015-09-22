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
NetworkPortalDetector* NetworkPortalDetector::network_portal_detector_ =
    nullptr;

// static
void NetworkPortalDetector::InitializeForTesting(
    NetworkPortalDetector* network_portal_detector) {
  if (network_portal_detector) {
    CHECK(!set_for_testing_)
        << "NetworkPortalDetector::InitializeForTesting is called twice";
    delete network_portal_detector_;
    network_portal_detector_ = network_portal_detector;
    set_for_testing_ = true;
  } else {
    network_portal_detector_ = nullptr;
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
  network_portal_detector_ = nullptr;
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

}  // namespace chromeos
