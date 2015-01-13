// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler.h"
#include "content/public/browser/devtools_http_handler.h"

namespace content {
namespace devtools {
namespace tethering {

// This class implements reversed tethering handler.
class TetheringHandler {
 public:
  using Response = DevToolsProtocolClient::Response;

  TetheringHandler(DevToolsHttpHandler::ServerSocketFactory* delegate,
                   scoped_refptr<base::MessageLoopProxy> message_loop_proxy);
  ~TetheringHandler();

  void SetClient(scoped_ptr<Client> client);

  Response Bind(DevToolsCommandId command_id, int port);
  Response Unbind(DevToolsCommandId command_id, int port);

 private:
  class TetheringImpl;

  void Accepted(uint16 port, const std::string& name);
  bool Activate();

  void SendBindSuccess(DevToolsCommandId command_id);
  void SendUnbindSuccess(DevToolsCommandId command_id);
  void SendInternalError(DevToolsCommandId command_id,
                         const std::string& message);

  scoped_ptr<Client> client_;
  DevToolsHttpHandler::ServerSocketFactory* socket_factory_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  bool is_active_;
  base::WeakPtrFactory<TetheringHandler> weak_factory_;

  static TetheringImpl* impl_;

  DISALLOW_COPY_AND_ASSIGN(TetheringHandler);
};

}  // namespace tethering
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_
