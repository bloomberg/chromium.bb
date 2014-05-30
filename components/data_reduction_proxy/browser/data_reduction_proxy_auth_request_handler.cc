// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "net/base/auth.h"

namespace {
// The minimum interval allowed, in milliseconds, between data reduction proxy
// auth requests.
const int64 kMinAuthRequestIntervalMs = 500;

// The minimum interval allowed, in milliseconds, between data reduction proxy
// auth token invalidation.
const int64 kMinTokenInvalidationIntervalMs = 60 * 60 * 1000;

// The maximum number of data reduction proxy authentication failures to
// accept before giving up.
const int kMaxBackToBackFailures = 5;
}

namespace data_reduction_proxy {

int64 DataReductionProxyAuthRequestHandler::auth_request_timestamp_ = 0;

int DataReductionProxyAuthRequestHandler::back_to_back_failure_count_ = 0;

int64
DataReductionProxyAuthRequestHandler::auth_token_invalidation_timestamp_ = 0;


DataReductionProxyAuthRequestHandler::DataReductionProxyAuthRequestHandler(
    DataReductionProxySettings* settings) : settings_(settings) {
}

DataReductionProxyAuthRequestHandler::~DataReductionProxyAuthRequestHandler() {
}

DataReductionProxyAuthRequestHandler::TryHandleResult
DataReductionProxyAuthRequestHandler::TryHandleAuthentication(
    net::AuthChallengeInfo* auth_info,
    base::string16* user,
    base::string16* password) {
  if (!auth_info) {
    return TRY_HANDLE_RESULT_IGNORE;
  }
  DCHECK(user);
  DCHECK(password);

  if (!IsAcceptableAuthChallenge(auth_info)) {
    *user = base::string16();
    *password = base::string16();
    return TRY_HANDLE_RESULT_IGNORE;
  }

  base::TimeTicks auth_request =
      base::TimeTicks::FromInternalValue(auth_request_timestamp_);
  base::TimeTicks auth_token_invalidation =
      base::TimeTicks::FromInternalValue(auth_token_invalidation_timestamp_);


  base::TimeTicks now = Now();
  if ((now - auth_request).InMilliseconds() < kMinAuthRequestIntervalMs) {
    // We've received back-to-back failures. There are two possibilities:
    // 1) Our auth token has expired and we should invalidate it, or
    // 2) We're receiving spurious failures from the service.
    //
    // If we haven't recently invalidated our token, we do that here
    // and make several attempts to authenticate. Otherwise, we fail.
    back_to_back_failure_count_++;
    if ((now - auth_token_invalidation).InMilliseconds() <
        kMinTokenInvalidationIntervalMs) {
      auth_token_invalidation_timestamp_ = now.ToInternalValue();
      back_to_back_failure_count_ = 0;
    } else {
      if (back_to_back_failure_count_ > kMaxBackToBackFailures) {
        DLOG(WARNING) << "Interpreting frequent data reduction proxy auth "
            << "requests as an authorization failure.";
        back_to_back_failure_count_ = 0;
        *user = base::string16();
        *password = base::string16();
        return TRY_HANDLE_RESULT_CANCEL;
      }
    }
  } else {
    back_to_back_failure_count_ = 0;
  }
  auth_request_timestamp_ = now.ToInternalValue();

  *password = GetTokenForAuthChallenge(auth_info);

  if (*password == base::string16()) {
    *user = base::string16();
    DLOG(WARNING) << "Data reduction proxy auth produced null token.";
    return TRY_HANDLE_RESULT_CANCEL;
  }
  *user = base::UTF8ToUTF16("fw-cookie");
  return TRY_HANDLE_RESULT_PROCEED;
}

bool DataReductionProxyAuthRequestHandler::IsAcceptableAuthChallenge(
    net::AuthChallengeInfo* auth_info) {
  return DataReductionProxySettings::IsAcceptableAuthChallenge(auth_info);
}

base::string16 DataReductionProxyAuthRequestHandler::GetTokenForAuthChallenge(
    net::AuthChallengeInfo* auth_info) {
  DCHECK(settings_);
  return settings_->GetTokenForAuthChallenge(auth_info);
}

base::TimeTicks DataReductionProxyAuthRequestHandler::Now() {
  return base::TimeTicks::Now();
}

}  // namespace data_reduction_proxy
