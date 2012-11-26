// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/mock_browser_plugin.h"
#include "content/renderer/render_process_impl.h"

namespace content {

MockBrowserPlugin::MockBrowserPlugin(
    int id,
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params)
    : BrowserPlugin(id, render_view, frame, params),
      transport_dib_next_sequence_number_(0) {
}

MockBrowserPlugin::~MockBrowserPlugin() {}

TransportDIB* MockBrowserPlugin::CreateTransportDIB(const size_t size) {
  return TransportDIB::Create(size, transport_dib_next_sequence_number_++);
}

void MockBrowserPlugin::FreeDamageBuffer(TransportDIB** damage_buffer) {
  DCHECK(*damage_buffer);
  RenderProcess::current()->FreeTransportDIB(*damage_buffer);
  *damage_buffer = NULL;
}

}  // namespace content
