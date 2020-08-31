// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_SPECIFICS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_SPECIFICS_H_

#include <string>

namespace chromeos {

// This structure encapsulates some specific parameters of signin flows that are
// not general enough to be put to UserContext.
struct SigninSpecifics {
  SigninSpecifics() {}

  // Specifies url that should be shown during Guest signin.
  std::string guest_mode_url;

  // Specifies if locale should be passed to guest mode url.
  bool guest_mode_url_append_locale = false;

  // Controls diagnostic mode for Kiosk App signin.
  bool kiosk_diagnostic_mode = false;

  // Whether it is an automatic login.
  bool is_auto_login = false;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_SPECIFICS_H_
