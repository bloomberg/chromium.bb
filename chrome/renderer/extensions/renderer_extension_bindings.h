// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
#pragma once

#include <string>

class ExtensionDispatcher;
class RenderView;

namespace v8 {
class Extension;
}

// This class adds extension-related javascript bindings to a renderer.  It is
// used by both web renderers and extension processes.
class RendererExtensionBindings {
 public:
  // Creates an instance of the extension.
  static v8::Extension* Get(ExtensionDispatcher* dispatcher);

  // Delivers a message sent using content script messaging. If
  // restrict_to_render_view is specified, only contexts in that render view
  // will receive the message.
  static void DeliverMessage(int target_port_id, const std::string& message,
                             RenderView* restrict_to_render_view);
};

#endif  // CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
