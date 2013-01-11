// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_COMPOSITING_HELPER_H_
#define  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_COMPOSITING_HELPER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ui/gfx/size.h"

namespace cc {
class TextureLayer;
}

namespace WebKit {
class WebPluginContainer;
class WebLayer;
}

namespace content {

class CONTENT_EXPORT BrowserPluginCompositingHelper {
 public:
  BrowserPluginCompositingHelper(WebKit::WebPluginContainer* container,
                                 int host_routing_id);
  ~BrowserPluginCompositingHelper();

  void EnableCompositing(bool);
  void OnBuffersSwapped(const gfx::Size& size,
                        const std::string& mailbox_name,
                        int gpu_route_id,
                        int gpu_host_id);

 private:
  int host_routing_id_;
  bool last_mailbox_valid_;

  scoped_refptr<cc::TextureLayer> texture_layer_;
  scoped_ptr<WebKit::WebLayer> web_layer_;
  WebKit::WebPluginContainer* container_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_COMPOSITING_HELPER_H_
