// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler_impl.h"

namespace content {

class DevToolsHttpHandlerDelegate;

namespace devtools {
namespace tethering {

// This class implements reversed tethering handler.
class TetheringHandler {
 public:
  TetheringHandler(DevToolsHttpHandlerDelegate* delegate,
                   scoped_refptr<base::MessageLoopProxy> message_loop_proxy);
  ~TetheringHandler();

  void SetClient(scoped_ptr<Client> client);

  scoped_refptr<DevToolsProtocol::Response> Bind(
      int port, scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> Unbind(
      int port, scoped_refptr<DevToolsProtocol::Command> command);

 private:
  class TetheringImpl;

  void Accepted(int port, const std::string& name);
  bool Activate();

  void SendBindSuccess(scoped_refptr<DevToolsProtocol::Command> command);
  void SendUnbindSuccess(scoped_refptr<DevToolsProtocol::Command> command);
  void SendInternalError(scoped_refptr<DevToolsProtocol::Command> command,
                         const std::string& message);

  scoped_ptr<Client> client_;
  DevToolsHttpHandlerDelegate* delegate_;
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
