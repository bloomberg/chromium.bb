// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_network_screen.h"

namespace chromeos {

using ::testing::AtLeast;
using ::testing::NotNull;

MockNetworkScreen::MockNetworkScreen(ScreenObserver* observer)
    : NetworkScreen(observer, new MockNetworkScreenActor) {
}

MockNetworkScreen::~MockNetworkScreen() {
}

MockNetworkScreenActor::MockNetworkScreenActor() {
  EXPECT_CALL(*this, SetDelegate(NotNull())).Times(AtLeast(1));
}

MockNetworkScreenActor::~MockNetworkScreenActor() {
}

}  // namespace chromeos
