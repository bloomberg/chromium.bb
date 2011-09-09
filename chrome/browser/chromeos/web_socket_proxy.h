// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_SOCKET_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_WEB_SOCKET_PROXY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

struct sockaddr;

namespace chromeos {

class WebSocketProxy {
 public:
  static const size_t kReadBufferLimit = 12 * 1024 * 1024;

  // Limits incoming websocket headers in initial stage of connection.
  static const size_t kHeaderLimit = 32 * 1024;

  // Limits number of simultaneously open connections.
  static const size_t kConnPoolLimit = 40;

  // Empty |allowed_origins| vector disables check for origin.
  WebSocketProxy(
      const std::vector<std::string>& allowed_origins,
      struct sockaddr* addr, int addr_len);
  ~WebSocketProxy();

  // Do not call it twice.
  void Run();

  // Terminates running server (should be called on a different thread).
  void Shutdown();

  // Call this on network change.
  void OnNetworkChange();

 private:
  void* impl_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketProxy);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEB_SOCKET_PROXY_H_
