// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_USER_NAMES_H_
#define CHROMEOS_LOGIN_USER_NAMES_H_

#include <string>

#include "chromeos/chromeos_export.h"

class AccountId;

namespace chromeos {

namespace login {

// Username for stub login when not running on ChromeOS.
CHROMEOS_EXPORT extern const char kStubUser[];

// Magic e-mail addresses are bad. They exist here because some code already
// depends on them and it is hard to figure out what. Any user types added in
// the future should be identified by a new |UserType|, not a new magic e-mail
// address.
// Username for Guest session user.
CHROMEOS_EXPORT extern const char kGuestUserName[];

// Domain that is used for all supervised users.
CHROMEOS_EXPORT extern const char kSupervisedUserDomain[];

// Canonicalizes a GAIA user ID, accounting for the legacy guest mode user ID
// which does trips up gaia::CanonicalizeEmail() because it does not contain an
// @ symbol.
CHROMEOS_EXPORT std::string CanonicalizeUserID(const std::string& user_id);

CHROMEOS_EXPORT const AccountId& StubAccountId();

// AccountId for the login screen. It identifies ephemeral profile that is used
// to display WebUI during OOBE and SignIn.
CHROMEOS_EXPORT const AccountId& SignInAccountId();

CHROMEOS_EXPORT const AccountId& GuestAccountId();

CHROMEOS_EXPORT const AccountId& DemoAccountId();

}  // namespace login

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_USER_NAMES_H_
