// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_eula_screen.h"

namespace chromeos {

using ::testing::AtLeast;
using ::testing::NotNull;

MockEulaScreen::MockEulaScreen(ScreenObserver* screen_observer,
                               EulaScreenActor* actor)
    : EulaScreen(screen_observer, actor) {
}

MockEulaScreen::~MockEulaScreen() {
}

void MockEulaScreenActor::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  MockSetDelegate(delegate);
}

MockEulaScreenActor::MockEulaScreenActor() {
  EXPECT_CALL(*this, MockSetDelegate(NotNull())).Times(AtLeast(1));
}

MockEulaScreenActor::~MockEulaScreenActor() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

}  // namespace chromeos
