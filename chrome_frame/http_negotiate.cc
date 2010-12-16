// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/http_negotiate.h"

#include <atlbase.h>
#include <atlcom.h>
#include <htiframe.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/exception_barrier.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/urlmon_url_request.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/vtable_patch_manager.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

const char kUACompatibleHttpHeader[] = "x-ua-compatible";
const char kLowerCaseUserAgent[] = "user-agent";

// From the latest urlmon.h. Symbol name prepended with LOCAL_ to
// avoid conflict (and therefore build errors) for those building with
// a newer Windows SDK.
// TODO(robertshield): Remove this once we update our SDK version.
const int LOCAL_BINDSTATUS_SERVER_MIMETYPEAVAILABLE = 54;

std::string AppendCFUserAgentString(LPCWSTR headers,
                                    LPCWSTR additional_headers) {
  using net::HttpUtil;

  std::string ascii_headers;
  if (additional_headers) {
    ascii_headers = WideToASCII(additional_headers);
  }

  // Extract "User-Agent" from |additional_headers| or |headers|.
  HttpUtil::HeadersIterator headers_iterator(ascii_headers.begin(),
                                             ascii_headers.end(), "\r\n");
  std::string user_agent_value;
  if (headers_iterator.AdvanceTo(kLowerCaseUserAgent)) {
    user_agent_value = headers_iterator.values();
  } else if (headers != NULL) {
    // See if there's a user-agent header specified in the original headers.
    std::string original_headers(WideToASCII(headers));
    HttpUtil::HeadersIterator original_it(original_headers.begin(),
        original_headers.end(), "\r\n");
    if (original_it.AdvanceTo(kLowerCaseUserAgent))
      user_agent_value = original_it.values();
  }

  // Use the default "User-Agent" if none was provided.
  if (user_agent_value.empty())
    user_agent_value = http_utils::GetDefaultUserAgent();

  // Now add chromeframe to it.
  user_agent_value = http_utils::AddChromeFrameToUserAgentValue(
      user_agent_value);

  // Build new headers, skip the existing user agent value from
  // existing headers.
  std::string new_headers;
  headers_iterator.Reset();
  while (headers_iterator.GetNext()) {
    std::string name(headers_iterator.name());
    if (!LowerCaseEqualsASCII(name, kLowerCaseUserAgent)) {
      new_headers += name + ": " + headers_iterator.values() + "\r\n";
    }
  }

  new_headers += "User-Agent: " + user_agent_value;
  new_headers += "\r\n";
  return new_headers;
}

std::string ReplaceOrAddUserAgent(LPCWSTR headers,
                                  const std::string& user_agent_value) {
  DCHECK(headers);
  using net::HttpUtil;

  std::string new_headers;
  if (headers) {
    std::string ascii_headers(WideToASCII(headers));

    // Extract "User-Agent" from the headers.
    HttpUtil::HeadersIterator headers_iterator(ascii_headers.begin(),
                                               ascii_headers.end(), "\r\n");

    // Build new headers, skip the existing user agent value from
    // existing headers.
    while (headers_iterator.GetNext()) {
      std::string name(headers_iterator.name());
      if (!LowerCaseEqualsASCII(name, kLowerCaseUserAgent)) {
        new_headers += name + ": " + headers_iterator.values() + "\r\n";
      }
    }
  }
  new_headers += "User-Agent: " + user_agent_value;
  new_headers += "\r\n";
  return new_headers;
}

