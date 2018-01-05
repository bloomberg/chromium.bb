// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/loader_util.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_response.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/http_raw_request_response_info.h"

namespace content {

namespace {
constexpr char kAcceptHeader[] = "Accept";
constexpr char kFrameAcceptHeader[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,"
    "image/apng,*/*;q=0.8";
constexpr char kStylesheetAcceptHeader[] = "text/css,*/*;q=0.1";
constexpr char kImageAcceptHeader[] = "image/webp,image/apng,image/*,*/*;q=0.8";
constexpr char kDefaultAcceptHeader[] = "*/*";
}  //  namespace

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

scoped_refptr<network::HttpRawRequestResponseInfo> BuildRawRequestResponseInfo(
    const net::URLRequest& request,
    const net::HttpRawRequestHeaders& raw_request_headers,
    const net::HttpResponseHeaders* raw_response_headers) {
  scoped_refptr<network::HttpRawRequestResponseInfo> info =
      new network::HttpRawRequestResponseInfo();

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

  if (!raw_response_headers)
    raw_response_headers = request.response_headers();
  if (raw_response_headers) {
    info->http_status_code = raw_response_headers->response_code();
    info->http_status_text = raw_response_headers->GetStatusText();

    std::string name;
    std::string value;
    for (size_t it = 0;
         raw_response_headers->EnumerateHeaderLines(&it, &name, &value);) {
      info->response_headers.push_back(std::make_pair(name, value));
    }
    if (report_headers_text) {
      info->response_headers_text =
          net::HttpUtil::ConvertHeadersBackToHTTPResponse(
              raw_response_headers->raw_headers());
    }
  }
  return info;
}

void AttachAcceptHeader(ResourceType type, net::URLRequest* request) {
  const char* accept_value = nullptr;
  switch (type) {
    case RESOURCE_TYPE_MAIN_FRAME:
    case RESOURCE_TYPE_SUB_FRAME:
      accept_value = kFrameAcceptHeader;
      break;
    case RESOURCE_TYPE_STYLESHEET:
      accept_value = kStylesheetAcceptHeader;
      break;
    case RESOURCE_TYPE_FAVICON:
    case RESOURCE_TYPE_IMAGE:
      accept_value = kImageAcceptHeader;
      break;
    case RESOURCE_TYPE_SCRIPT:
    case RESOURCE_TYPE_FONT_RESOURCE:
    case RESOURCE_TYPE_SUB_RESOURCE:
    case RESOURCE_TYPE_OBJECT:
    case RESOURCE_TYPE_MEDIA:
    case RESOURCE_TYPE_WORKER:
    case RESOURCE_TYPE_SHARED_WORKER:
    case RESOURCE_TYPE_PREFETCH:
    case RESOURCE_TYPE_XHR:
    case RESOURCE_TYPE_PING:
    case RESOURCE_TYPE_SERVICE_WORKER:
    case RESOURCE_TYPE_CSP_REPORT:
    case RESOURCE_TYPE_PLUGIN_RESOURCE:
      accept_value = kDefaultAcceptHeader;
      break;
    case RESOURCE_TYPE_LAST_TYPE:
      NOTREACHED();
      break;
  }
  // The false parameter prevents overwriting an existing accept header value,
  // which is needed because JS can manually set an accept header on an XHR.
  request->SetExtraRequestHeaderByName(kAcceptHeader, accept_value, false);
}

int BuildLoadFlagsForRequest(const ResourceRequest& request) {
  int load_flags = request.load_flags;

  // Although EV status is irrelevant to sub-frames and sub-resources, we have
  // to perform EV certificate verification on all resources because an HTTP
  // keep-alive connection created to load a sub-frame or a sub-resource could
  // be reused to load a main frame.
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (request.resource_type == RESOURCE_TYPE_MAIN_FRAME) {
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;
  } else if (request.resource_type == RESOURCE_TYPE_PREFETCH) {
    load_flags |= net::LOAD_PREFETCH;
  }

  return load_flags;
}

}  // namespace content
