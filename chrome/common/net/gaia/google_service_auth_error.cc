// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/google_service_auth_error.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "net/base/net_errors.h"

GoogleServiceAuthError::Captcha::Captcha(
    const std::string& t, const GURL& img, const GURL& unlock)
    : token(t), image_url(img), unlock_url(unlock) {}

bool GoogleServiceAuthError::operator==(
    const GoogleServiceAuthError &b) const {
  return (state_ == b.state_ &&
          network_error_ == b.network_error_ &&
          captcha_.token == b.captcha_.token &&
          captcha_.image_url == b.captcha_.image_url &&
          captcha_.unlock_url == b.captcha_.unlock_url);
}

GoogleServiceAuthError::GoogleServiceAuthError(State s)
    : state_(s),
      captcha_("", GURL(), GURL()),
      network_error_(0) {
  // If the caller has no idea, then we just set it to a generic failure.
  if (s == CONNECTION_FAILED) {
    network_error_ = net::ERR_FAILED;
  }
}

GoogleServiceAuthError
    GoogleServiceAuthError::FromConnectionError(int error) {
  return GoogleServiceAuthError(CONNECTION_FAILED, error);
}

GoogleServiceAuthError GoogleServiceAuthError::FromCaptchaChallenge(
    const std::string& captcha_token,
    const GURL& captcha_image_url,
    const GURL& captcha_unlock_url) {
  return GoogleServiceAuthError(CAPTCHA_REQUIRED, captcha_token,
                                captcha_image_url, captcha_unlock_url);
}

GoogleServiceAuthError GoogleServiceAuthError::None() {
  return GoogleServiceAuthError(NONE);
}

const GoogleServiceAuthError::State& GoogleServiceAuthError::state() const {
  return state_;
}

const GoogleServiceAuthError::Captcha& GoogleServiceAuthError::captcha() const {
  return captcha_;
}

int GoogleServiceAuthError::network_error() const {
  return network_error_;
}

DictionaryValue* GoogleServiceAuthError::ToValue() const {
  DictionaryValue* value = new DictionaryValue();
  std::string state_str;
  switch (state_) {
#define STATE_CASE(x) case x: state_str = #x; break
    STATE_CASE(NONE);
    STATE_CASE(INVALID_GAIA_CREDENTIALS);
    STATE_CASE(USER_NOT_SIGNED_UP);
    STATE_CASE(CONNECTION_FAILED);
    STATE_CASE(CAPTCHA_REQUIRED);
    STATE_CASE(ACCOUNT_DELETED);
    STATE_CASE(ACCOUNT_DISABLED);
    STATE_CASE(SERVICE_UNAVAILABLE);
    STATE_CASE(TWO_FACTOR);
    STATE_CASE(REQUEST_CANCELED);
    STATE_CASE(HOSTED_NOT_ALLOWED);
#undef STATE_CASE
    default:
      NOTREACHED();
      break;
  }
  value->SetString("state", state_str);
  if (state_ == CAPTCHA_REQUIRED) {
    DictionaryValue* captcha_value = new DictionaryValue();
    value->Set("captcha", captcha_value);
    captcha_value->SetString("token", captcha_.token);
    captcha_value->SetString("imageUrl", captcha_.image_url.spec());
    captcha_value->SetString("unlockUrl", captcha_.unlock_url.spec());
  } else if (state_ == CONNECTION_FAILED) {
    value->SetString("networkError", net::ErrorToString(network_error_));
  }
  return value;
}

GoogleServiceAuthError::GoogleServiceAuthError(State s, int error)
    : state_(s),
      captcha_("", GURL(), GURL()),
      network_error_(error) {}

GoogleServiceAuthError::GoogleServiceAuthError(
    State s, const std::string& captcha_token,
    const GURL& captcha_image_url,
    const GURL& captcha_unlock_url)
    : state_(s),
      captcha_(captcha_token, captcha_image_url, captcha_unlock_url),
      network_error_(0) {}
