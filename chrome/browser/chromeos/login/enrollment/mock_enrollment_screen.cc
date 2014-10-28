// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"

namespace chromeos {

MockEnrollmentScreen::MockEnrollmentScreen(
    BaseScreenDelegate* base_screen_delegate,
    EnrollmentScreenActor* actor)
    : EnrollmentScreen(base_screen_delegate, actor) {
}

MockEnrollmentScreen::~MockEnrollmentScreen() {
}

MockEnrollmentScreenActor::MockEnrollmentScreenActor() {
}

MockEnrollmentScreenActor::~MockEnrollmentScreenActor() {
}

}  // namespace chromeos
