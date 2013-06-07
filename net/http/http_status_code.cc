// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_status_code.h"

#include "base/logging.h"

namespace net {

const char* GetHttpReasonPhrase(HttpStatusCode code) {
  switch (code) {
    case HTTP_CONTINUE:
      return "Continue";
    case HTTP_OK:
      return "OK";
    case HTTP_PARTIAL_CONTENT:
      return "Partial Content";
    case HTTP_FORBIDDEN:
      return "Forbidden";
    case HTTP_NOT_FOUND:
      return "Not Found";
    case HTTP_METHOD_NOT_ALLOWED:
      return "Method Not Allowed";
    case HTTP_REQUESTED_RANGE_NOT_SATISFIABLE:
      return "Range Not Satisfiable";
    case HTTP_INTERNAL_SERVER_ERROR:
      return "Internal Server Error";
    default:
      NOTREACHED();
  }

  return "";
}

}  // namespace net
