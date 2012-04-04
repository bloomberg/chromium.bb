// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mock_chromeos_network.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

namespace {

MockChromeOSNetwork* g_mock_chromeos_network = NULL;

// Calls mock SetNetworkServicePropertyGValue.
void CallMockSetNetworkServicePropertyGValue(const char* service_path,
                                             const char* property,
                                             const GValue* gvalue) {
  g_mock_chromeos_network->SetNetworkServicePropertyGValue(service_path,
                                                           property, gvalue);
}

// Calls mock SetNetworkDevicePropertyGValue.
void CallMockSetNetworkDevicePropertyGValue(const char* device_path,
                                            const char* property,
                                            const GValue* gvalue) {
  g_mock_chromeos_network->SetNetworkDevicePropertyGValue(device_path,
                                                          property, gvalue);
}

// Calls mock SetNetworkIPConfigPropertyGValue.
void CallMockSetNetworkIPConfigPropertyGValue(const char* ipconfig_path,
                                              const char* property,
                                              const GValue* gvalue) {
  g_mock_chromeos_network->SetNetworkIPConfigPropertyGValue(ipconfig_path,
                                                            property, gvalue);
}

// Calls mock SetNetworkManagerPropertyGValue.
void CallMockSetNetworkManagerPropertyGValue(const char* property,
                                             const GValue* gvalue) {
  g_mock_chromeos_network->SetNetworkManagerPropertyGValue(property, gvalue);
}

// Calls mock ConfigureService.
void CallMockConfigureService(const char* identifier,
                              const GHashTable* properties,
                              NetworkActionCallback callback,
                              void* object) {
  g_mock_chromeos_network->ConfigureService(identifier, properties, callback,
                                            object);
}

}  // namespace

MockChromeOSNetwork::MockChromeOSNetwork() {}

MockChromeOSNetwork::~MockChromeOSNetwork() {}

// static
void MockChromeOSNetwork::Initialize() {
  if (g_mock_chromeos_network) {
    LOG(WARNING) << "MockChromeOSNetwork is already initialized.";
    return;
  }
  g_mock_chromeos_network = new MockChromeOSNetwork;

  if (!CrosLibrary::Get()) {
    chromeos::SetNetworkServicePropertyGValue =
      &CallMockSetNetworkServicePropertyGValue;
    chromeos::SetNetworkDevicePropertyGValue =
      &CallMockSetNetworkDevicePropertyGValue;
    chromeos::SetNetworkIPConfigPropertyGValue =
      &CallMockSetNetworkIPConfigPropertyGValue;
    chromeos::SetNetworkManagerPropertyGValue =
      &CallMockSetNetworkManagerPropertyGValue;
    chromeos::ConfigureService = &CallMockConfigureService;
  } else {
    LOG(ERROR) << "CrosLibrary is initialized.";
  }
}

// static
void MockChromeOSNetwork::Shutdown() {
  if (!CrosLibrary::Get()) {
    chromeos::SetNetworkServicePropertyGValue = NULL;
    chromeos::SetNetworkDevicePropertyGValue = NULL;
    chromeos::SetNetworkIPConfigPropertyGValue = NULL;
    chromeos::SetNetworkManagerPropertyGValue = NULL;
    chromeos::ConfigureService = NULL;
  } else {
    LOG(ERROR) << "CrosLibrary is initialized.";
  }

  delete g_mock_chromeos_network;
  g_mock_chromeos_network = NULL;
}

// static
MockChromeOSNetwork* MockChromeOSNetwork::Get() {
  return g_mock_chromeos_network;
}

}  // namespace chromeos
