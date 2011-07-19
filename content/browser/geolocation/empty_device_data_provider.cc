// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/empty_device_data_provider.h"

// No platform has (cellular) radio data provider yet.
// static
template<>
RadioDataProviderImplBase* RadioDataProvider::DefaultFactoryFunction() {
  return new EmptyDeviceDataProvider<RadioData>();
}

// Only define for platforms that lack a real wifi data provider.
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_LINUX)
// static
template<>
WifiDataProviderImplBase* WifiDataProvider::DefaultFactoryFunction() {
  return new EmptyDeviceDataProvider<WifiData>();
}
#endif

// Only define for platforms that lack a real gateway data provider.
#if !defined(OS_LINUX) && !defined(OS_WIN)
// static
template<>
GatewayDataProviderImplBase* GatewayDataProvider::DefaultFactoryFunction() {
  return new EmptyDeviceDataProvider<GatewayData>();
}
#endif
