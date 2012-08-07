// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DEVTOOLS_SERVER_H_
#define CHROME_BROWSER_ANDROID_DEVTOOLS_SERVER_H_

#include <string>
#include "base/basictypes.h"

namespace content {
class DevToolsHttpHandler;
}

// This class controls Developer Tools remote debugging server.
class DevToolsServer {
 public:
  DevToolsServer();
  ~DevToolsServer();

  // Opens linux abstract socket to be ready for remote debugging.
  void Start();

  // Closes debugging socket, stops debugging.
  void Stop();

  bool IsStarted() const;

 private:
  content::DevToolsHttpHandler* protocol_handler_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsServer);
};

#endif  // CHROME_BROWSER_ANDROID_DEVTOOLS_SERVER_H_
