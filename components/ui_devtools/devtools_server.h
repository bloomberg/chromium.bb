// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_DEVTOOLS_SERVER_H_
#define COMPONENTS_UI_DEVTOOLS_DEVTOOLS_SERVER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/Forward.h"
#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/devtools_client.h"
#include "components/ui_devtools/devtools_export.h"
#include "components/ui_devtools/string_util.h"
#include "net/server/http_server.h"

namespace ui_devtools {

class UI_DEVTOOLS_EXPORT UiDevToolsServer
    : public NON_EXPORTED_BASE(net::HttpServer::Delegate) {
 public:
  ~UiDevToolsServer() override;

  // Returns an empty unique_ptr if ui devtools flag isn't enabled or if a
  // server instance has already been created.
  static std::unique_ptr<UiDevToolsServer> Create(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner);

  // Returns a list of attached UiDevToolsClient name + URL
  using NameUrlPair = std::pair<std::string, std::string>;
  static std::vector<NameUrlPair> GetClientNamesAndUrls();

  void AttachClient(std::unique_ptr<UiDevToolsClient> client);
  void SendOverWebSocket(int connection_id, const String& message);

 private:
  explicit UiDevToolsServer(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner);

  void Start(const std::string& address_string, uint16_t port);
  void StartServer(const std::string& address_string, uint16_t port);

  // HttpServer::Delegate
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, const std::string& data) override;
  void OnClose(int connection_id) override;

  using ClientsList = std::vector<std::unique_ptr<UiDevToolsClient>>;
  using ConnectionsMap = std::map<uint32_t, UiDevToolsClient*>;
  ClientsList clients_;
  ConnectionsMap connections_;

  std::unique_ptr<base::Thread> thread_;
  std::unique_ptr<net::HttpServer> server_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // The server (owned by ash for now)
  static UiDevToolsServer* devtools_server_;

  DISALLOW_COPY_AND_ASSIGN(UiDevToolsServer);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_DEVTOOLS_SERVER_H_
