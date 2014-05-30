// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"


namespace net {
class AuthChallengeInfo;
}

namespace data_reduction_proxy {

class DataReductionProxySettings;

class DataReductionProxyAuthRequestHandler {
 public:
  enum TryHandleResult {
    TRY_HANDLE_RESULT_IGNORE,
    TRY_HANDLE_RESULT_PROCEED,
    TRY_HANDLE_RESULT_CANCEL
  };

  // Constructs an authentication request handler and takes a pointer to a
  // |settings| object, which must outlive the handler.
  explicit DataReductionProxyAuthRequestHandler(
      DataReductionProxySettings* settings);
  virtual ~DataReductionProxyAuthRequestHandler();

  // Returns |PROCEED| if the authentication challenge provided is one that the
  // data reduction proxy should handle and |IGNORE| if not. Returns |CANCEL| if
  // there are a string of |MAX_BACK_TO_BACK_FAILURES| successive retries.
  TryHandleResult TryHandleAuthentication(net::AuthChallengeInfo* auth_info,
                                          base::string16* user,
                                          base::string16* password);

 protected:
  // Visible for testing.
  virtual bool IsAcceptableAuthChallenge(net::AuthChallengeInfo* auth_info);

  // Visible for testing.
  virtual base::string16 GetTokenForAuthChallenge(
      net::AuthChallengeInfo* auth_info);

  // Visible for testing.
  virtual base::TimeTicks Now();

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyAuthRequestHandlerTest,
                           CancelAfterSuccessiveAuthAttempts);



  // System timestamp of the last data reduction proxy authentication request.
  // This is used to cancel data reduction proxy auth requests that are denied
  // rather than loop forever trying a rejected token.
  static int64 auth_request_timestamp_;

  // The number of back to back data reduction proxy authentication failures
  // that occurred with no more than |MIN_AUTH_REQUEST_INTERVAL_MS| between each
  // adjacent pair of them.
  static int back_to_back_failure_count_;

  // System timestamp of the last data reduction proxy auth token invalidation.
  // This is used to expire old tokens on back-to-back failures, and distinguish
  // invalidation from repeat failures due to the client not being authorized.
  static int64 auth_token_invalidation_timestamp_;

  // Settings object for the data reduction proxy. Must outlive the handler.
  DataReductionProxySettings* settings_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyAuthRequestHandler);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_
