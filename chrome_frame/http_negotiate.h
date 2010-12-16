// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_HTTP_NEGOTIATE_H_
#define CHROME_FRAME_HTTP_NEGOTIATE_H_

#include <shdeprecated.h>
#include <string>
#include <urlmon.h>

#include "base/basictypes.h"
#include "base/scoped_comptr_win.h"

// From the latest urlmon.h. Symbol name prepended with LOCAL_ to
// avoid conflict (and therefore build errors) for those building with
// a newer Windows SDK.
// TODO(robertshield): Remove this once we update our SDK version.
extern const int LOCAL_BINDSTATUS_SERVER_MIMETYPEAVAILABLE;

// Scans |additional_headers| and |headers| for User-Agent header or grabs
// the default User-Agent if none is found. Append "chromeframe" at the end
// of the string. Returns the original content of |additional_headers| but
// with the new User-Agent header. Somewhat unusual interface is dictated
// because we use it in IHttpNegotiate::BeginningTransaction.
// The function is a public since we want to use it from
// UrlmonUrlRequest::BeginningTransaction for the unusual but yet possible case
// when |headers| contains an User-Agent string.
std::string AppendCFUserAgentString(LPCWSTR headers,
                                    LPCWSTR additional_headers);

// Adds or replaces the User-Agent header in a set of HTTP headers.
// Arguments are the same as with AppendCFUserAgentString.
std::string ReplaceOrAddUserAgent(LPCWSTR headers,
                                  const std::string& user_agent_value);

#endif  // CHROME_FRAME_HTTP_NEGOTIATE_H_
