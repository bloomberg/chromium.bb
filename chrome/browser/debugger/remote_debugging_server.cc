// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/remote_debugging_server.h"

#include "chrome/browser/debugger/browser_list_tabcontents_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "content/public/browser/devtools_http_handler.h"

RemoteDebuggingServer::RemoteDebuggingServer(Profile* profile,
                                             const std::string& ip,
                                             int port,
                                             const std::string& frontend_url) {
  // Initialize DevTools data source.
  DevToolsUI::RegisterDevToolsDataSource(profile);

  devtools_http_handler_ =
      content::DevToolsHttpHandler::Start(ip,
                                          port,
                                          frontend_url,
                                          new BrowserListTabContentsProvider());
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
  devtools_http_handler_->Stop();
}
