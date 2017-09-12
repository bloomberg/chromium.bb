// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_JS_RENDERER_MESSAGING_SERVICE_H_
#define EXTENSIONS_RENDERER_JS_RENDERER_MESSAGING_SERVICE_H_

#include <string>

#include "base/macros.h"

struct ExtensionMsg_ExternalConnectionInfo;
struct ExtensionMsg_TabConnectionInfo;

namespace content {
class RenderFrame;
}

namespace extensions {
class ScriptContextSet;
struct Message;
struct PortId;

// The messaging service to handle dispatching extension messages and connection
// events to different contexts.
class JSRendererMessagingService {
 public:
  JSRendererMessagingService();
  ~JSRendererMessagingService();

  // Checks whether the port exists in the given frame. If it does not, a reply
  // is sent back to the browser.
  void ValidateMessagePort(const ScriptContextSet& context_set,
                           const PortId& port_id,
                           content::RenderFrame* render_frame);

  // Dispatches the onConnect content script messaging event to some contexts
  // in |context_set|. If |restrict_to_render_frame| is specified, only contexts
  // in that render frame will receive the message.
  void DispatchOnConnect(const ScriptContextSet& context_set,
                         const PortId& target_port_id,
                         const std::string& channel_name,
                         const ExtensionMsg_TabConnectionInfo& source,
                         const ExtensionMsg_ExternalConnectionInfo& info,
                         const std::string& tls_channel_id,
                         content::RenderFrame* restrict_to_render_frame);

  // Delivers a message sent using content script messaging to some of the
  // contexts in |bindings_context_set|. If |restrict_to_render_frame| is
  // specified, only contexts in that render view will receive the message.
  void DeliverMessage(const ScriptContextSet& context_set,
                      const PortId& target_port_id,
                      const Message& message,
                      content::RenderFrame* restrict_to_render_frame);

  // Dispatches the onDisconnect event in response to the channel being closed.
  void DispatchOnDisconnect(const ScriptContextSet& context_set,
                            const PortId& port_id,
                            const std::string& error_message,
                            content::RenderFrame* restrict_to_render_frame);

 private:
  DISALLOW_COPY_AND_ASSIGN(JSRendererMessagingService);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_JS_RENDERER_MESSAGING_SERVICE_H_
