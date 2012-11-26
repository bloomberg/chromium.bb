// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_MOCK_BROWSER_PLUGIN_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_MOCK_BROWSER_PLUGIN_H_

#include "content/renderer/browser_plugin/browser_plugin.h"

namespace content {

class MockBrowserPlugin : public BrowserPlugin {
 public:
  MockBrowserPlugin(
      int id,
      RenderViewImpl* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

  virtual ~MockBrowserPlugin();

  virtual TransportDIB* CreateTransportDIB(const size_t size) OVERRIDE;
  virtual void FreeDamageBuffer(TransportDIB** damage_buffer) OVERRIDE;

  // Allow poking at a few private members.
  using BrowserPlugin::guest_crashed_;
  using BrowserPlugin::pending_damage_buffer_;

 private:
  int transport_dib_next_sequence_number_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_MOCK_BROWSER_PLUGIN_H_
