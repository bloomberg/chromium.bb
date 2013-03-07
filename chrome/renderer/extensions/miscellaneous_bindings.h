// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_MISCELLANEOUS_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_MISCELLANEOUS_BINDINGS_H_

#include <string>

#include "chrome/renderer/extensions/chrome_v8_context_set.h"


namespace content {
class RenderView;
}

namespace v8 {
class Extension;
}

namespace extensions {
class ChromeV8Extension;
class Dispatcher;

// Manually implements some random JavaScript bindings for the extension system.
//
// TODO(aa): This should all get re-implemented using SchemaGeneratedBindings.
// If anything needs to be manual for some reason, it should be implemented in
// its own class.
class MiscellaneousBindings {
 public:
  // Creates an instance of the extension.
  static ChromeV8Extension* Get(Dispatcher* dispatcher);

  // Dispatches the Port.onConnect content script messaging event to some
  // contexts in |contexts|. If |restrict_to_render_view| is specified, only
  // contexts in that render view will receive the message.
  static void DispatchOnConnect(
      const ChromeV8ContextSet::ContextSet& contexts,
      int target_port_id,
      const std::string& channel_name,
      const std::string& tab_json,
      const std::string& source_extension_id,
      const std::string& target_extension_id,
      content::RenderView* restrict_to_render_view);

  // Delivers a message sent using content script messaging to some of the
  // contexts in |bindings_context_set|. If |restrict_to_render_view| is
  // specified, only contexts in that render view will receive the message.
  static void DeliverMessage(
      const ChromeV8ContextSet::ContextSet& context_set,
      int target_port_id,
      const std::string& message,
      content::RenderView* restrict_to_render_view);

  // Dispatches the Port.onDisconnect event in response to the channel being
  // closed.
  static void DispatchOnDisconnect(
      const ChromeV8ContextSet::ContextSet& context_set,
      int port_id,
      bool connection_error,
      content::RenderView* restrict_to_render_view);
};

}  // namespace

#endif  // CHROME_RENDERER_EXTENSIONS_MISCELLANEOUS_BINDINGS_H_
