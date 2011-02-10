// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A GoogleServiceAuthError is immutable, plain old data representing an
// error from an attempt to authenticate with a Google service.
// It could be from Google Accounts itself, or any service using Google
// Accounts (e.g expired credentials).  It may contain additional data such as
// captcha challenges.

// A GoogleServiceAuthError without additional data is just a State, defined
// below. A case could be made to have this relation implicit, to allow raising
// error events concisely by doing OnAuthError(GoogleServiceAuthError::NONE),
// for example. But the truth is this class is ever so slightly more than a
// transparent wrapper around 'State' due to additional Captcha data
// (e.g consider operator=), and this would violate the style guide. Thus,
// you must explicitly use the constructor when all you have is a State.
// The good news is the implementation nests the enum inside a class, so you
// may forward declare and typedef GoogleServiceAuthError to something shorter
// in the comfort of your own translation unit.

#ifndef CHROME_COMMON_NET_GAIA_GOOGLE_SERVICE_AUTH_ERROR_H_
#define CHROME_COMMON_NET_GAIA_GOOGLE_SERVICE_AUTH_ERROR_H_
#pragma once

#include <string>

#include "googleurl/src/gurl.h"

class DictionaryValue;

class GoogleServiceAuthError {
 public:
  //
  // These enumerations are referenced by integer value in HTML login code.
  // Do not change the numeric values.
  //
  enum State {
    // The user is authenticated.
    NONE = 0,

    // The credentials supplied to GAIA were either invalid, or the locally
    // cached credentials have expired.
    INVALID_GAIA_CREDENTIALS = 1,

    // The GAIA user is not authorized to use the service.
    USER_NOT_SIGNED_UP = 2,

    // Could not connect to server to verify credentials. This could be in
    // response to either failure to connect to GAIA or failure to connect to
    // the service needing GAIA tokens during authentication.
    CONNECTION_FAILED = 3,

    // The user needs to satisfy a CAPTCHA challenge to unlock their account.
    // If no other information is available, this can be resolved by visiting
    // https://www.google.com/accounts/DisplayUnlockCaptcha. Otherwise,
    // captcha() will provide details about the associated challenge.
    CAPTCHA_REQUIRED = 4,

    // The user account has been deleted.
    ACCOUNT_DELETED = 5,

    // The user account has been disabled.
    ACCOUNT_DISABLED = 6,

    // The service is not available; try again later.
    SERVICE_UNAVAILABLE = 7,

    // The password is valid but we need two factor to get a token.
    TWO_FACTOR = 8,

    // The requestor of the authentication step cancelled the request
    // prior to completion.
    REQUEST_CANCELED = 9,

    // The user has provided a HOSTED account, when this service requires
    // a GOOGLE account.
    HOSTED_NOT_ALLOWED = 10,
  };

  // Additional data for CAPTCHA_REQUIRED errors.
  struct Captcha {
    Captcha(const std::string& t, const GURL& img, const GURL& unlock);
    std::string token;  // Globally identifies the specific CAPTCHA challenge.
    GURL image_url;     // The CAPTCHA image to show the user.
    GURL unlock_url;    // Pretty unlock page containing above captcha.
  };

  // For test only.
  bool operator==(const GoogleServiceAuthError &b) const;

  // Construct a GoogleServiceAuthError from a State with no additional data.
  explicit GoogleServiceAuthError(State s);

  // Construct a GoogleServiceAuthError from a network error.
  // It will be created with CONNECTION_FAILED set.
  static GoogleServiceAuthError FromConnectionError(int error);

  // Construct a CAPTCHA_REQUIRED error with CAPTCHA challenge data.
  static GoogleServiceAuthError FromCaptchaChallenge(
      const std::string& captcha_token,
      const GURL& captcha_image_url,
      const GURL& captcha_unlock_url);

  // Provided for convenience for clients needing to reset an instance to NONE.
  // (avoids err_ = GoogleServiceAuthError(GoogleServiceAuthError::NONE), due
  // to explicit class and State enum relation. Note: shouldn't be inlined!
  static GoogleServiceAuthError None();

  // The error information.
  const State& state() const;
  const Captcha& captcha() const;
  int network_error() const;

  // Returns info about this object in a dictionary.  Caller takes
  // ownership of returned dictionary.
  DictionaryValue* ToValue() const;

 private:
  GoogleServiceAuthError(State s, int error);

  GoogleServiceAuthError(State s, const std::string& captcha_token,
                         const GURL& captcha_image_url,
                         const GURL& captcha_unlock_url);

  State state_;
  Captcha captcha_;
  int network_error_;
};

#endif  // CHROME_COMMON_NET_GAIA_GOOGLE_SERVICE_AUTH_ERROR_H_
