// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "content/common/content_client.h"

class RenderView;
class SkBitmap;

namespace WebKit {
class WebFrame;
class WebPlugin;
class WebURLRequest;
struct WebPluginParams;
struct WebURLError;
}

namespace content {

// Embedder API for participating in renderer logic.
class ContentRendererClient {
 public:
  virtual SkBitmap* GetSadPluginBitmap();
  virtual std::string GetDefaultEncoding();
  // Create a plugin in the given frame.  Can return NULL, in which case
  // RenderView will create a plugin itself.
  virtual WebKit::WebPlugin* CreatePlugin(
      RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);
  // Returns the html to display when a navigation error occurs.
  virtual std::string GetNavigationErrorHtml(
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error);
  // Returns the ISO 639_1 language code of the specified |text|, or 'unknown'
  // if it failed.
  virtual std::string DetermineTextLanguage(const string16& text);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
