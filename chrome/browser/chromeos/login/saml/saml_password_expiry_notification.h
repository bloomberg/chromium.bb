// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SAML_SAML_PASSWORD_EXPIRY_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SAML_SAML_PASSWORD_EXPIRY_NOTIFICATION_H_

class Profile;

namespace chromeos {

// Utility functions to show or hide a password expiry notification.

// Shows a password expiry notification. |lessThanNDays| should be 1 if the
// password expires in less than 1 day, 0 if it has already expired, etc.
// Negative numbers are treated the same as zero.
void ShowSamlPasswordExpiryNotification(Profile* profile, int lessThanNDays);

// Hides the password expiry notification if it is currently shown.
void DismissSamlPasswordExpiryNotification(Profile* profile);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SAML_SAML_PASSWORD_EXPIRY_NOTIFICATION_H_
