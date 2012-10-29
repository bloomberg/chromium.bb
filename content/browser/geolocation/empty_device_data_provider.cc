// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/empty_device_data_provider.h"

namespace content {

// Only define for platforms that lack a real wifi data provider.
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_LINUX)
// static
template<>
WifiDataProviderImplBase* WifiDataProvider::DefaultFactoryFunction() {
  return new EmptyDeviceDataProvider<WifiData>();
}
#endif

}  // namespace content
