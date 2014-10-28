// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_error_screen.h"

namespace chromeos {

MockErrorScreen::MockErrorScreen(BaseScreenDelegate* base_screen_delegate,
                                 ErrorScreenActor* actor)
    : ErrorScreen(base_screen_delegate, actor) {
}

MockErrorScreen::~MockErrorScreen() {
}

MockErrorScreenActor::MockErrorScreenActor() {
}

MockErrorScreenActor::~MockErrorScreenActor() {
}

}  // namespace chromeos
