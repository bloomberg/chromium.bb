// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_update_screen.h"

namespace chromeos {

MockUpdateScreen::MockUpdateScreen(ScreenObserver* screen_observer)
    : UpdateScreen(screen_observer, new MockUpdateScreenActor) {
}

MockUpdateScreen::~MockUpdateScreen() {
}

MockUpdateScreenActor::MockUpdateScreenActor() {
}

MockUpdateScreenActor::~MockUpdateScreenActor() {
}

}  // namespace chromeos
