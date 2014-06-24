// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin_specifics.h"

namespace chromeos {

SigninSpecifics::SigninSpecifics()
    : guest_mode_url_append_locale(false), kiosk_diagnostic_mode(false) {
}

}  // namespace chromeos
