// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_provider.h"

#if defined(OS_MACOSX)
#include "device/generic_sensor/platform_sensor_provider_mac.h"
#elif defined(OS_ANDROID)
#include "device/generic_sensor/platform_sensor_provider_android.h"
#endif

namespace {

static device::PlatformSensorProvider* g_provider_for_testing = nullptr;

}  // namespace

namespace device {

// static
void PlatformSensorProvider::SetProviderForTesting(
    PlatformSensorProvider* provider) {
  g_provider_for_testing = provider;
}

// static
PlatformSensorProvider* PlatformSensorProvider::GetInstance() {
  if (g_provider_for_testing)
    return g_provider_for_testing;
#if defined(OS_MACOSX)
  return PlatformSensorProviderMac::GetInstance();
#elif defined(OS_ANDROID)
  return PlatformSensorProviderAndroid::GetInstance();
#else
  return nullptr;
#endif
}

}  // namespace device
