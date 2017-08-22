// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/loader_util.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "content/public/common/resource_devtools_info.h"
#include "content/public/common/resource_response.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

namespace content {

bool ShouldSniffContent(net::URLRequest* url_request,
                        ResourceResponse* response) {
  const std::string& mime_type = response->head.mime_type;

  std::string content_type_options;
  url_request->GetResponseHeaderByName("x-content-type-options",
                                       &content_type_options);

  bool sniffing_blocked =
      base::LowerCaseEqualsASCII(content_type_options, "nosniff");
  bool we_would_like_to_sniff =
      net::ShouldSniffMimeType(url_request->url(), mime_type);

  if (!sniffing_blocked && we_would_like_to_sniff) {
    // We're going to look at the data before deciding what the content type
    // is.  That means we need to delay sending the response started IPC.
    VLOG(1) << "To buffer: " << url_request->url().spec();
    return true;
  }

  return false;
}

scoped_refptr<ResourceDevToolsInfo> BuildDevToolsInfo(
    const net::URLRequest& request,
    const net::HttpRawRequestHeaders& raw_request_headers) {
  scoped_refptr<ResourceDevToolsInfo> info = new ResourceDevToolsInfo();

  const net::HttpResponseInfo& response_info = request.response_info();
  // Unparsed headers only make sense if they were sent as text, i.e. HTTP 1.x.
  bool report_headers_text =
      !response_info.DidUseQuic() && !response_info.was_fetched_via_spdy;

  for (const auto& pair : raw_request_headers.headers())
    info->request_headers.push_back(pair);
  std::string request_line = raw_request_headers.request_line();
  if (report_headers_text && !request_line.empty()) {
    std::string text = std::move(request_line);
    for (const auto& pair : raw_request_headers.headers()) {
      if (!pair.second.empty()) {
        base::StringAppendF(&text, "%s: %s\r\n", pair.first.c_str(),
                            pair.second.c_str());
      } else {
        base::StringAppendF(&text, "%s:\r\n", pair.first.c_str());
      }
    }
    info->request_headers_text = std::move(text);
  }

  const net::HttpResponseHeaders* response_headers = request.response_headers();
  if (response_headers) {
    info->http_status_code = response_headers->response_code();
    info->http_status_text = response_headers->GetStatusText();

    std::string name;
    std::string value;
    for (size_t it = 0;
         response_headers->EnumerateHeaderLines(&it, &name, &value);) {
      info->response_headers.push_back(std::make_pair(name, value));
    }
    if (report_headers_text) {
      info->response_headers_text =
          net::HttpUtil::ConvertHeadersBackToHTTPResponse(
              response_headers->raw_headers());
    }
  }
  return info;
}

}  // namespace content
