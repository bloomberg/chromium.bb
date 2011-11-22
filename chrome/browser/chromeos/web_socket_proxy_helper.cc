// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_socket_proxy_helper.h"

#include "base/string_number_conversions.h"
#include "base/logging.h"

namespace chromeos {

// static
bool WebSocketProxyHelper::FetchPassportAddrNamePort(
      uint8* begin, uint8* end,
      std::string* passport, std::string* addr,
      std::string* hostname, int* port) {
  std::string input(begin, end);

  // At first, get rid of a colon at the end.
  if (input.empty() || input[input.length() - 1] != ':')
    return false;
  input.erase(input.length() - 1);

  // Fetch the passport from the start.
  if (!FetchToken(true, false, &input, passport) || passport->empty())
    return false;

  // Fetch the IP address from the start. Match '['-brackets if presented.
  if (!FetchToken(true, true, &input, addr))
    return false;

  // Fetch the port number from the end.
  std::string port_str;
  if (!FetchToken(false, false, &input, &port_str) || port_str.empty() ||
      !base::StringToInt(port_str, port) ||
      *port <= 0 || *port >= (1<<16))
    return false;

  // The hostname is everything remained.
  if (input.length() < 2 || input[input.length() - 1] != ':')
    return false;
  hostname->assign(input, 0, input.length() - 1);

  return true;
}

// static
bool WebSocketProxyHelper::FetchToken(bool forward,
                                      bool match_brackets,
                                      std::string* input,
                                      std::string* token ) {
  if (input->empty()) {
    token->clear();
  } else {
    std::string separator =
        (forward && match_brackets && input->at(0) == '[') ? "]:" : ":";
    size_t pos = forward ? input->find(separator) : input->rfind(separator);
    if (pos != std::string::npos) {
      size_t start = forward ? 0 : pos + 1;
      size_t end = forward ? pos : input->length();
      token->assign(*input, start, end - start + separator.length() - 1);
      input->erase(start, end - start + separator.length());
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace chromeos
