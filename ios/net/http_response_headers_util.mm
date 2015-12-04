// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/http_response_headers_util.h"

#include <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

namespace {
// String format used to create the http status line from the status code and
// its localized description.
NSString* const kHttpStatusLineFormat = @"HTTP %ld %s";
// String format used to pass the header name/value pairs to the
// HttpResponseHeaders.
NSString* const kHeaderLineFormat = @"%@: %@";
}

namespace net {

const std::string kDummyHttpStatusDescription = "DummyStatusDescription";

scoped_refptr<HttpResponseHeaders> CreateHeadersFromNSHTTPURLResponse(
    NSHTTPURLResponse* response) {
  DCHECK(response);
  // Create the status line and initialize the headers.
  NSInteger status_code = response.statusCode;
  std::string status_line = base::SysNSStringToUTF8([NSString
      stringWithFormat:kHttpStatusLineFormat, static_cast<long>(status_code),
                       kDummyHttpStatusDescription.c_str()]);
  scoped_refptr<HttpResponseHeaders> http_headers(
      new HttpResponseHeaders(status_line));
  // Iterate through |response|'s headers and add them to |http_headers|.
  [response.allHeaderFields
      enumerateKeysAndObjectsUsingBlock:^(NSString* header_name,
                                          NSString* value, BOOL*) {
        NSString* header_line =
            [NSString stringWithFormat:kHeaderLineFormat, header_name, value];
        http_headers->AddHeader(base::SysNSStringToUTF8(header_line));
      }];
  return http_headers;
}

}  // namespae net
