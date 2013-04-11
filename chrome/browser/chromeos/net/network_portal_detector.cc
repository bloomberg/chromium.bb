// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/chromeos/net/network_portal_detector_stub.h"
#include "chrome/common/chrome_switches.h"

namespace chromeos {

namespace {

NetworkPortalDetector* g_network_portal_detector = NULL;

bool IsTestMode() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType);
}

}  // namespace

NetworkPortalDetector::NetworkPortalDetector() {
}

NetworkPortalDetector::~NetworkPortalDetector() {
}

// static
NetworkPortalDetector* NetworkPortalDetector::CreateInstance() {
  DCHECK(!g_network_portal_detector);
  CHECK(NetworkPortalDetector::IsEnabledInCommandLine());
  if (IsTestMode()) {
    g_network_portal_detector = new NetworkPortalDetectorStub();
  } else {
    CHECK(g_browser_process);
    CHECK(g_browser_process->system_request_context());
    g_network_portal_detector = new NetworkPortalDetectorImpl(
        g_browser_process->system_request_context());
  }
  return g_network_portal_detector;
}

// static
NetworkPortalDetector* NetworkPortalDetector::GetInstance() {
  if (!NetworkPortalDetector::IsEnabledInCommandLine())
    return NULL;
  if (!g_network_portal_detector)
    return CreateInstance();
  return g_network_portal_detector;
}

// static
bool NetworkPortalDetector::IsEnabledInCommandLine() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableChromeCaptivePortalDetector);
}

}  // namespace chromeos
