// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/mock_enterprise_enrollment_screen.h"

namespace chromeos {

MockEnterpriseEnrollmentScreen::MockEnterpriseEnrollmentScreen(
    ScreenObserver* screen_observer, EnterpriseEnrollmentScreenActor* actor)
    : EnterpriseEnrollmentScreen(screen_observer, actor) {
}

MockEnterpriseEnrollmentScreen::~MockEnterpriseEnrollmentScreen() {
}

MockEnterpriseEnrollmentScreenActor::MockEnterpriseEnrollmentScreenActor() {
}

MockEnterpriseEnrollmentScreenActor::~MockEnterpriseEnrollmentScreenActor() {
}

}  // namespace chromeos
