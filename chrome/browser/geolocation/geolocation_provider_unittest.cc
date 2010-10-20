// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_provider.h"

#include "base/singleton.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class GeolocationProviderTest : public testing::Test {
 protected:
  // TODO(joth): This test should inject a mock arbitrator rather than cause
  // the real one to be instantiated. For now, this is not an issue as the
  // arbitrator is never started.
  GeolocationProviderTest()
      : provider_(new GeolocationProvider) {}

  ~GeolocationProviderTest() {
    DefaultSingletonTraits<GeolocationProvider>::Delete(provider_);
  }
  // Message loop for main thread, as provider depends on it existing.
  MessageLoop message_loop_;

  // Object under tests. Owned, but not a scoped_ptr due to specialized
  // destruction protocol.
  GeolocationProvider* provider_;
};

// Regression test for http://crbug.com/59377
TEST_F(GeolocationProviderTest, OnPermissionGrantedWithoutObservers) {
  EXPECT_FALSE(provider_->HasPermissionBeenGranted());
  provider_->OnPermissionGranted(GURL("http://example.com"));
  EXPECT_TRUE(provider_->HasPermissionBeenGranted());
}

}  // namespace
