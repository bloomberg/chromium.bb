// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"

class GURL;

namespace safe_browsing {

// A request for checking if an unfamiliar login form or a password reuse event
// is safe. PasswordProtectionRequest objects are owned by
// PasswordProtectionService indicated by |password_protection_service_|.
class PasswordProtectionRequest : public net::URLFetcherDelegate {
 public:
  // The outcome of the request. These values are used for UMA.
  // DO NOT CHANGE THE ORDERING OF THESE VALUES.
  enum RequestOutcome {
    UNKNOWN = 0,
    SUCCEEDED = 1,
    CANCELED = 2,
    TIMEDOUT = 3,
    MATCHED_WHITELIST = 4,
    RESPONSE_ALREADY_CACHED = 5,
    NO_EXTENDED_REPORTING = 6,
    INCOGNITO = 7,
    REQUEST_MALFORMED = 8,
    FETCH_FAILED = 9,
    RESPONSE_MALFORMED = 10,
    SERVICE_DESTROYED = 11,
    MAX_OUTCOME
  };

  PasswordProtectionRequest(const GURL& main_frame_url,
                            LoginReputationClientRequest::TriggerType type,
                            bool is_extended_reporting,
                            bool is_incognito,
                            base::WeakPtr<PasswordProtectionService> pps,
                            int request_timeout_in_ms);

  ~PasswordProtectionRequest() override;

  base::WeakPtr<PasswordProtectionRequest> GetWeakPtr() {
    return weakptr_factory_.GetWeakPtr();
  }

  // Starts processing request by checking extended reporting and incognito
  // conditions.
  void Start();

  // Cancels the current request. |timed_out| indicates if this cancellation is
  // due to timeout. This function will call Finish() to destroy |this|.
  void Cancel(bool timed_out);

  // net::URLFetcherDelegate override.
  // Processes the received response.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  GURL main_frame_url() const { return main_frame_url_; }

  bool is_incognito() const { return is_incognito_; }

 private:
  // If |main_frame_url_| matches whitelist, call Finish() immediately;
  // otherwise call CheckCachedVerdicts().
  void OnWhitelistCheckDone(bool match_whitelist);

  // Looks up cached verdicts. If verdict is already cached, call SendRequest();
  // otherwise call Finish().
  void CheckCachedVerdicts();

  // Initiates network request to Safe Browsing backend.
  void SendRequest();

  // Start a timer to cancel the request if it takes too long.
  void StartTimeout();

  // |this| will be destroyed after calling this function.
  void Finish(RequestOutcome outcome,
              std::unique_ptr<LoginReputationClientResponse> response);

  void CheckWhitelistsOnUIThread();

  // Main frame URL of the login form.
  GURL main_frame_url_;

  // If this request is for unfamiliar login page or for a password reuse event.
  const LoginReputationClientRequest::TriggerType request_type_;

  // If user is opted-in Safe Browsing Extended Reporting.
  const bool is_extended_reporting_;

  // If current session is in incognito mode.
  const bool is_incognito_;

  // When request is sent.
  base::TimeTicks request_start_time_;

  // URLFetcher instance for sending request and receiving response.
  std::unique_ptr<net::URLFetcher> fetcher_;

  // The PasswordProtectionService instance owns |this|.
  base::WeakPtr<PasswordProtectionService> password_protection_service_;

  // If we haven't receive response after this period of time, we cancel this
  // request.
  const int request_timeout_in_ms_;

  base::WeakPtrFactory<PasswordProtectionRequest> weakptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionRequest);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
