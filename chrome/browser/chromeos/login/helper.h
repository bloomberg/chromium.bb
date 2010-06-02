// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by Chromium OS login.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_

namespace views {
class Throbber;
}  // namespace views

namespace chromeos {

// Creates default smoothed throbber for time consuming operations on login.
views::Throbber* CreateDefaultSmoothedThrobber();

// Creates default throbber.
views::Throbber* CreateDefaultThrobber();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
