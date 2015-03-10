// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"

namespace net {

namespace {

void AddSpdyHeader(const std::string& name,
                   const std::string& value,
                   SpdyHeaderBlock* headers) {
  if (headers->find(name) == headers->end()) {
    (*headers)[name] = value;
  } else {
    (*headers)[name] += '\0' + value;
  }
}

} // namespace

bool SpdyHeadersToHttpResponse(const SpdyHeaderBlock& headers,
                               SpdyMajorVersion protocol_version,
                               HttpResponseInfo* response) {
  std::string status_key = (protocol_version >= SPDY3) ? ":status" : "status";
  std::string version_key =
      (protocol_version >= SPDY3) ? ":version" : "version";
  std::string version;
  std::string status;

  // The "status" header is required. "version" is required below SPDY4.
  SpdyHeaderBlock::const_iterator it;
  it = headers.find(status_key);
  if (it == headers.end())
    return false;
  status = it->second;

  if (protocol_version >= SPDY4) {
    version = "HTTP/1.1";
  } else {
    it = headers.find(version_key);
    if (it == headers.end())
      return false;
    version = it->second;
  }
  std::string raw_headers(version);
  raw_headers.push_back(' ');
  raw_headers.append(status);
  raw_headers.push_back('\0');
  for (it = headers.begin(); it != headers.end(); ++it) {
    // For each value, if the server sends a NUL-separated
    // list of values, we separate that back out into
    // individual headers for each value in the list.
    // e.g.
    //    Set-Cookie "foo\0bar"
    // becomes
    //    Set-Cookie: foo\0
    //    Set-Cookie: bar\0
    std::string value = it->second;
    size_t start = 0;
    size_t end = 0;
    do {
      end = value.find('\0', start);
      std::string tval;
      if (end != value.npos)
        tval = value.substr(start, (end - start));
      else
        tval = value.substr(start);
      if (protocol_version >= 3 && it->first[0] == ':')
        raw_headers.append(it->first.substr(1));
      else
        raw_headers.append(it->first);
      raw_headers.push_back(':');
      raw_headers.append(tval);
      raw_headers.push_back('\0');
      start = end + 1;
    } while (end != value.npos);
  }

  response->headers = new HttpResponseHeaders(raw_headers);
  response->was_fetched_via_spdy = true;
  return true;
}

void CreateSpdyHeadersFromHttpRequest(const HttpRequestInfo& info,
                                      const HttpRequestHeaders& request_headers,
                                      SpdyMajorVersion protocol_version,
                                      bool direct,
                                      SpdyHeaderBlock* headers) {

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = base::StringToLowerASCII(it.name());
    if (name == "connection" || name == "proxy-connection" ||
        name == "transfer-encoding" || name == "host") {
      continue;
    }
    AddSpdyHeader(name, it.value(), headers);
  }
  static const char kHttpProtocolVersion[] = "HTTP/1.1";

  if (protocol_version < SPDY3) {
    (*headers)["version"] = kHttpProtocolVersion;
    (*headers)["method"] = info.method;
    (*headers)["host"] = GetHostAndOptionalPort(info.url);
    (*headers)["scheme"] = info.url.scheme();
    if (direct)
      (*headers)["url"] = HttpUtil::PathForRequest(info.url);
    else
      (*headers)["url"] = HttpUtil::SpecForRequest(info.url);
  } else {
    if (protocol_version < SPDY4) {
      (*headers)[":version"] = kHttpProtocolVersion;
      (*headers)[":host"] = GetHostAndOptionalPort(info.url);
    } else {
      (*headers)[":authority"] = GetHostAndOptionalPort(info.url);
    }
    (*headers)[":method"] = info.method;
    (*headers)[":scheme"] = info.url.scheme();
    (*headers)[":path"] = HttpUtil::PathForRequest(info.url);
  }
}

void CreateSpdyHeadersFromHttpResponse(
    const HttpResponseHeaders& response_headers,
    SpdyMajorVersion protocol_version,
    SpdyHeaderBlock* headers) {
  std::string status_key = (protocol_version >= SPDY3) ? ":status" : "status";
  std::string version_key =
      (protocol_version >= SPDY3) ? ":version" : "version";

  const std::string status_line = response_headers.GetStatusLine();
  std::string::const_iterator after_version =
      std::find(status_line.begin(), status_line.end(), ' ');
  if (protocol_version < SPDY4) {
    (*headers)[version_key] = std::string(status_line.begin(), after_version);
  }
  (*headers)[status_key] = std::string(after_version + 1, status_line.end());

  void* iter = NULL;
  std::string raw_name, value;
  while (response_headers.EnumerateHeaderLines(&iter, &raw_name, &value)) {
    std::string name = base::StringToLowerASCII(raw_name);
    AddSpdyHeader(name, value, headers);
  }
}

static_assert(HIGHEST - LOWEST < 4 && HIGHEST - MINIMUM_PRIORITY < 5,
              "request priority incompatible with spdy");

SpdyPriority ConvertRequestPriorityToSpdyPriority(
    const RequestPriority priority,
    SpdyMajorVersion protocol_version) {
  DCHECK_GE(priority, MINIMUM_PRIORITY);
  DCHECK_LE(priority, MAXIMUM_PRIORITY);
  return static_cast<SpdyPriority>(MAXIMUM_PRIORITY - priority);
}

NET_EXPORT_PRIVATE RequestPriority ConvertSpdyPriorityToRequestPriority(
    SpdyPriority priority,
    SpdyMajorVersion protocol_version) {
  // Handle invalid values gracefully.
  // Note that SpdyPriority is not an enum, hence the magic constants.
  return (priority >= 5) ?
      IDLE : static_cast<RequestPriority>(4 - priority);
}

GURL GetUrlFromHeaderBlock(const SpdyHeaderBlock& headers,
                           SpdyMajorVersion protocol_version,
                           bool pushed) {
  DCHECK_LE(SPDY3, protocol_version);
  SpdyHeaderBlock::const_iterator it = headers.find(":scheme");
  if (it == headers.end())
    return GURL();
  std::string url = it->second;
  url.append("://");

  it = headers.find(protocol_version >= SPDY4 ? ":authority" : ":host");
  if (it == headers.end())
    return GURL();
  url.append(it->second);

  it = headers.find(":path");
  if (it == headers.end())
    return GURL();
  url.append(it->second);
  return GURL(url);
}

}  // namespace net
