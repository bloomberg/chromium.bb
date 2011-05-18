// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_SOCKET_PROXY_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_WEB_SOCKET_PROXY_CONTROLLER_H_
#pragma once

#include <string>

namespace chromeos {

// Controls webproxy to TCP service.
class WebSocketProxyController {
 public:
  enum ConnectionFlags {
    PLAIN_TCP    = 0,
    TLS_OVER_TCP = 1 << 0
  };

  // Can be called on any thread. Subsequent calls are cheap and do nothing.
  static void Initiate();

  // All methods can be called on any thread.
  static void Shutdown();
  static bool IsInitiated();

  static bool CheckCredentials(
      const std::string& extension_id,
      const std::string& hostname,
      unsigned short port,
      ConnectionFlags);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEB_SOCKET_PROXY_CONTROLLER_H_
