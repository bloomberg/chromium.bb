// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most logic for the platform wifi provider is now factored into
// WifiDataProviderCommon and covered by it's unit tests.

#include "content/browser/geolocation/wifi_data_provider_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(GeolocationWifiDataProviderWinTest, CreateDestroy) {
  // WifiDataProviderCommon requires the client to have a message loop.
  base::MessageLoop dummy_loop;
  scoped_refptr<WifiDataProviderWin> instance(new WifiDataProviderWin);
  instance = NULL;
  SUCCEED();
  // Can't actually call start provider on the WifiDataProviderWin without
  // it accessing hardware and so risking making the test flaky.
}

}  // namespace content
