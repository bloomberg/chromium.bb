// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
#define CHROMECAST_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"

namespace devtools_http_handler {
class DevToolsHttpHandler;
}

namespace chromecast {
namespace shell {

class CastDevToolsManagerDelegate;

class RemoteDebuggingServer {
 public:
  RemoteDebuggingServer();
  ~RemoteDebuggingServer();

 private:
  // Called on port number changed.
  void OnPortChanged();

  // Returns whether or not the remote debugging server should be available
  // on device startup.
  bool ShouldStartImmediately();

  // TODO(dgozman): remove once devtools_discovery component is extracted.
  scoped_ptr<CastDevToolsManagerDelegate> manager_delegate_;
  scoped_ptr<devtools_http_handler::DevToolsHttpHandler> devtools_http_handler_;

  IntegerPrefMember pref_port_;
  uint16 port_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDebuggingServer);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
