// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_AUTHENTICATION_NOTIFICATION_DETAILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_AUTHENTICATION_NOTIFICATION_DETAILS_H_

// A class to hold the parameters we get back from an authentication attempt
// through the login manager
class AuthenticationNotificationDetails {
 public:
  explicit AuthenticationNotificationDetails(bool success) : success_(success) {
  }

  bool success() const { return success_; }

 private:
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticationNotificationDetails);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_AUTHENTICATION_NOTIFICATION_DETAILS_H_
