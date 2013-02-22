// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
#define CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/ui/host_desktop.h"

class Profile;

namespace content {
class DevToolsHttpHandler;
}

class RemoteDebuggingServer {
 public:
  RemoteDebuggingServer(Profile* profile,
                        chrome::HostDesktopType host_desktop_type,
                        const std::string& ip,
                        int port,
                        const std::string& frontend_url);

  virtual ~RemoteDebuggingServer();

 private:
  content::DevToolsHttpHandler* devtools_http_handler_;
  DISALLOW_COPY_AND_ASSIGN(RemoteDebuggingServer);
};

#endif  // CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
