// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_update_screen.h"

namespace chromeos {

using ::testing::AtLeast;
using ::testing::NotNull;

MockUpdateScreen::MockUpdateScreen(ScreenObserver* screen_observer,
                                   UpdateScreenActor* actor)
    : UpdateScreen(screen_observer, actor) {
}

MockUpdateScreen::~MockUpdateScreen() {
}

MockUpdateScreenActor::MockUpdateScreenActor()
    : screen_(NULL) {
  EXPECT_CALL(*this, MockSetDelegate(NotNull())).Times(AtLeast(1));
}

MockUpdateScreenActor::~MockUpdateScreenActor() {
  if (screen_)
    screen_->OnActorDestroyed(this);
}

void MockUpdateScreenActor::SetDelegate(UpdateScreenActor::Delegate* screen) {
  screen_ = screen;
  MockSetDelegate(screen);
}

}  // namespace chromeos
