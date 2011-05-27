// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_eula_screen.h"

namespace chromeos {

using ::testing::AtLeast;
using ::testing::NotNull;

MockEulaScreen::MockEulaScreen(ScreenObserver* screen_observer)
    : EulaScreen(screen_observer, new MockEulaScreenActor) {
}

MockEulaScreen::~MockEulaScreen() {
}

MockEulaScreenActor::MockEulaScreenActor() {
  EXPECT_CALL(*this, SetDelegate(NotNull())).Times(AtLeast(1));
}

MockEulaScreenActor::~MockEulaScreenActor() {
}

}  // namespace chromeos
