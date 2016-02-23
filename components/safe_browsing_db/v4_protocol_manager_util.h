// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_

// A class that implements the stateless methods used by the GetHashUpdate and
// GetFullHash stubby calls made by Chrome using the SafeBrowsing V4 protocol.

#include <string>

#include "base/gtest_prod_util.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace safe_browsing {

// Config passed to the constructor of a V4 protocol manager.
struct V4ProtocolConfig {
  // The safe browsing client name sent in each request.
  std::string client_name;

  // Current product version sent in each request.
  std::string version;

  // The Google API key.
  std::string key_param;
};

class V4ProtocolManagerUtil {
 public:
  // Record HTTP response code when there's no error in fetching an HTTP
  // request, and the error code, when there is.
  // |metric_name| is the name of the UMA metric to record the response code or
  // error code against, |status| represents the status of the HTTP request, and
  // |response code| represents the HTTP response code received from the server.
  static void RecordHttpResponseOrErrorCode(const char* metric_name,
                                            const net::URLRequestStatus& status,
                                            int response_code);

  // Generates a Pver4 request URL.
  // |request_base64| is the serialized request protocol buffer encoded in
  // base 64.
  // |method_name| is the name of the method to call, as specified in the proto,
  // |config| is an instance of V4ProtocolConfig that stores the client config.
  static GURL GetRequestUrl(const std::string& request_base64,
                            const std::string& method_name,
                            const V4ProtocolConfig& config);

  // Worker function for calculating the backoff times.
  // |multiplier| is doubled for each consecutive error after the
  // first, and |error_count| is incremented with each call.
  static base::TimeDelta GetNextBackOffInterval(size_t* error_count,
                                                size_t* multiplier);

 private:
  V4ProtocolManagerUtil() {};
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerUtilTest,
                           TestBackOffLogic);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerUtilTest,
                           TestGetRequestUrl);

  // Composes a URL using |prefix|, |method| (e.g.: encodedFullHashes).
  // |request_base64|, |client_id|, |version| and |key_param|. |prefix|
  // should contain the entire url prefix including scheme, host and path.
  static std::string ComposeUrl(const std::string& prefix,
                                const std::string& method,
                                const std::string& request_base64,
                                const std::string& client_id,
                                const std::string& version,
                                const std::string& key_param);

  DISALLOW_COPY_AND_ASSIGN(V4ProtocolManagerUtil);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_
