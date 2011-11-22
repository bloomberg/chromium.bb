// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_SOCKET_PROXY_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_WEB_SOCKET_PROXY_HELPER_H_
#pragma once

#include <string>
#include "base/basictypes.h"

namespace chromeos {

// Helper class for WebSocketProxy.
class WebSocketProxyHelper {
 public:
  // Parses "passport:addr:hostname:port:" string.  Returns true on success.
  static bool FetchPassportAddrNamePort(
      uint8* begin, uint8* end,
      std::string* passport, std::string* addr,
      std::string* hostname, int* port);

  // Fetches a token from the string and erases it. Token separtor is
  // either ':' or ']:' if the token is started with '[' and
  // |match_brackets|. Fetching position (start or end) is determined by
  // |forward|. Returns whether the token was successfully fetched.
  static bool FetchToken(bool forward, bool match_brackets,
                         std::string* input,
                         std::string* token);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEB_SOCKET_PROXY_HELPER_H_
