// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_TETHERING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_TETHERING_HANDLER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_protocol.h"

namespace content {

class DevToolsHttpHandlerDelegate;

// This class implements reversed tethering handler.
class TetheringHandler : public DevToolsProtocol::Handler {
 public:
  TetheringHandler(DevToolsHttpHandlerDelegate* delegate,
                   scoped_refptr<base::MessageLoopProxy> message_loop_proxy);
  ~TetheringHandler() override;

 private:
  class TetheringImpl;

  void Accepted(int port, const std::string& name);
  bool Activate();

  scoped_refptr<DevToolsProtocol::Response> OnBind(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> OnUnbind(
      scoped_refptr<DevToolsProtocol::Command>  command);

  void SendBindSuccess(scoped_refptr<DevToolsProtocol::Command> command);
  void SendUnbindSuccess(scoped_refptr<DevToolsProtocol::Command> command);
  void SendInternalError(scoped_refptr<DevToolsProtocol::Command> command,
                         const std::string& message);

  DevToolsHttpHandlerDelegate* delegate_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  bool is_active_;
  base::WeakPtrFactory<TetheringHandler> weak_factory_;

  static TetheringImpl* impl_;

  DISALLOW_COPY_AND_ASSIGN(TetheringHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_TETHERING_HANDLER_H_
