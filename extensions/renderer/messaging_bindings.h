// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_MESSAGING_BINDINGS_H_
#define EXTENSIONS_RENDERER_MESSAGING_BINDINGS_H_

#include <string>

#include "extensions/renderer/script_context_set.h"

struct ExtensionMsg_ExternalConnectionInfo;

namespace base {
class DictionaryValue;
}

namespace content {
class RenderView;
}

namespace v8 {
class Extension;
}

namespace extensions {
class Dispatcher;
struct Message;
class ObjectBackedNativeHandler;
class ScriptContextSet;

// Manually implements JavaScript bindings for extension messaging.
//
// TODO(aa): This should all get re-implemented using SchemaGeneratedBindings.
// If anything needs to be manual for some reason, it should be implemented in
// its own class.
class MessagingBindings {
 public:
  // Creates an instance of the extension.
  static ObjectBackedNativeHandler* Get(Dispatcher* dispatcher,
                                        ScriptContext* context);

  // Dispatches the onConnect content script messaging event to some contexts
  // in |context_set|. If |restrict_to_render_view| is specified, only contexts
  // in that render view will receive the message.
  static void DispatchOnConnect(const ScriptContextSet& context_set,
                                int target_port_id,
                                const std::string& channel_name,
                                const base::DictionaryValue& source_tab,
                                const ExtensionMsg_ExternalConnectionInfo& info,
                                const std::string& tls_channel_id,
                                content::RenderView* restrict_to_render_view);

  // Delivers a message sent using content script messaging to some of the
  // contexts in |bindings_context_set|. If |restrict_to_render_view| is
  // specified, only contexts in that render view will receive the message.
  static void DeliverMessage(const ScriptContextSet& context_set,
                             int target_port_id,
                             const Message& message,
                             content::RenderView* restrict_to_render_view);

  // Dispatches the onDisconnect event in response to the channel being closed.
  static void DispatchOnDisconnect(
      const ScriptContextSet& context_set,
      int port_id,
      const std::string& error_message,
      content::RenderView* restrict_to_render_view);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_MESSAGING_BINDINGS_H_
