// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_login_display.h"

#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/gfx/rect.h"

namespace chromeos {

MockLoginDisplay::MockLoginDisplay() : LoginDisplay(NULL, gfx::Rect()) {
}

MockLoginDisplay::~MockLoginDisplay() {
}

}  // namespace chromeos
