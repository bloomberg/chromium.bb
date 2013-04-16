// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_error_screen.h"

namespace chromeos {

MockErrorScreen::MockErrorScreen(ScreenObserver* screen_observer,
                                 ErrorScreenActor* actor)
    : ErrorScreen(screen_observer, actor) {
}

MockErrorScreen::~MockErrorScreen() {
}

MockErrorScreenActor::MockErrorScreenActor() {
}

MockErrorScreenActor::~MockErrorScreenActor() {
}

}  // namespace chromeosx
