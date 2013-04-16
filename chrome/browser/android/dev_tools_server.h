// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DEV_TOOLS_SERVER_H_
#define CHROME_BROWSER_ANDROID_DEV_TOOLS_SERVER_H_

#include <string>
#include <jni.h>
#include "base/basictypes.h"

namespace content {
class DevToolsHttpHandler;
}

// This class controls Developer Tools remote debugging server.
class DevToolsServer {
 public:
  DevToolsServer();
  DevToolsServer(bool use_bundled_frontend_resources,
                 const std::string& socket_name);
  ~DevToolsServer();

  // Opens linux abstract socket to be ready for remote debugging.
  void Start();

  // Closes debugging socket, stops debugging.
  void Stop();

  bool IsStarted() const;

 private:
  bool use_bundled_frontend_resources_;
  std::string socket_name_;
  content::DevToolsHttpHandler* protocol_handler_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsServer);
};

bool RegisterDevToolsServer(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_DEV_TOOLS_SERVER_H_
