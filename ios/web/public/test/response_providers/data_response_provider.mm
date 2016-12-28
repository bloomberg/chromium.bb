// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/response_providers/data_response_provider.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/third_party/gcdwebserver/src/GCDWebServer/Responses/GCDWebServerDataResponse.h"

namespace web {

GCDWebServerResponse* DataResponseProvider::GetGCDWebServerResponse(
    const Request& request) {
  std::string response_body;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
  GetResponseHeadersAndBody(request, &response_headers, &response_body);
  GCDWebServerDataResponse* data_response = [GCDWebServerDataResponse
      responseWithHTML:base::SysUTF8ToNSString(response_body)];
  data_response.statusCode = response_headers->response_code();
  size_t iter = 0;
  std::string name;
  std::string value;
  while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
    // TODO(crbug.com/435350): Extract out other names that can't be set by
    // using the |setValue:forAdditionalHeader:| API such as "ETag" etc.
    if (name == "Content-type") {
      data_response.contentType = base::SysUTF8ToNSString(value);
      continue;
    }
    [data_response setValue:base::SysUTF8ToNSString(value)
        forAdditionalHeader:base::SysUTF8ToNSString(name)];
  }
  return data_response;
}

}  // namespace web
