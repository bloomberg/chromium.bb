// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_network_screen.h"

namespace chromeos {

using ::testing::AtLeast;
using ::testing::NotNull;

MockNetworkScreen::MockNetworkScreen(ScreenObserver* observer,
                                     NetworkScreenActor* actor)
    : NetworkScreen(observer, actor) {
}

MockNetworkScreen::~MockNetworkScreen() {
}


MockNetworkScreenActor::MockNetworkScreenActor() {
  EXPECT_CALL(*this, MockSetDelegate(NotNull())).Times(AtLeast(1));
}

MockNetworkScreenActor::~MockNetworkScreenActor() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void MockNetworkScreenActor::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  MockSetDelegate(delegate);
}

}  // namespace chromeos
