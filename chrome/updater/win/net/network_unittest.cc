// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/network.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdaterTestNetwork, NetworkFetcherWinHTTPFactory) {
  auto fetcher = base::MakeRefCounted<NetworkFetcherWinHTTPFactory>()->Create();
  EXPECT_NE(nullptr, fetcher.get());
}

}  // namespace updater
