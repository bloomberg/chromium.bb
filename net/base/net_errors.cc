// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringize_macros.h"

namespace {

// Get all valid error codes into an array as positive numbers, for use in the
// |GetAllErrorCodesForUma| function below.
#define NET_ERROR(label, value) -(value),
const int kAllErrorCodes[] = {
#include "net/base/net_error_list.h"
};
#undef NET_ERROR

}  // namespace

namespace net {

const char kErrorDomain[] = "net";

std::string ErrorToString(int error) {
  return "net::" + ErrorToShortString(error);
}

std::string ErrorToShortString(int error) {
  if (error == 0)
    return "OK";

  const char* error_string;
  switch (error) {
#define NET_ERROR(label, value) \
  case ERR_ ## label: \
    error_string = # label; \
    break;
#include "net/base/net_error_list.h"
#undef NET_ERROR
  default:
    NOTREACHED();
    error_string = "<unknown>";
  }
  return std::string("ERR_") + error_string;
}

bool IsCertificateError(int error) {
  // Certificate errors are negative integers from net::ERR_CERT_BEGIN
  // (inclusive) to net::ERR_CERT_END (exclusive) in *decreasing* order.
  // ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN is currently an exception to this
  // rule.
  return (error <= ERR_CERT_BEGIN && error > ERR_CERT_END) ||
         (error == ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN);
}

std::vector<int> GetAllErrorCodesForUma() {
  return base::CustomHistogram::ArrayToCustomRanges(
      kAllErrorCodes, arraysize(kAllErrorCodes));
}

Error FileErrorToNetError(base::File::Error file_error) {
  switch (file_error) {
    case base::File::FILE_OK:
      return net::OK;
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return net::ERR_ACCESS_DENIED;
    case base::File::FILE_ERROR_INVALID_URL:
      return net::ERR_INVALID_URL;
    case base::File::FILE_ERROR_NOT_FOUND:
      return net::ERR_FILE_NOT_FOUND;
    default:
      return net::ERR_FAILED;
  }
}

}  // namespace net
