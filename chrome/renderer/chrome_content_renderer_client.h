// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#define CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include "content/renderer/content_renderer_client.h"

namespace webkit {
namespace npapi {
class PluginGroup;
}
}

namespace chrome {

class ChromeContentRendererClient : public content::ContentRendererClient {
 public:
  virtual SkBitmap* GetSadPluginBitmap();
  virtual std::string GetDefaultEncoding();
  virtual WebKit::WebPlugin* CreatePlugin(
      RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);
  virtual std::string GetNavigationErrorHtml(
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error);
  virtual std::string DetermineTextLanguage(const string16& text);
  virtual bool RunIdleHandlerWhenWidgetsHidden();
  virtual bool AllowPopup(const GURL& creator);

 private:
  WebKit::WebPlugin* CreatePluginPlaceholder(
      RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      const webkit::npapi::PluginGroup& group,
      int resource_id,
      int message_id,
      bool is_blocked_for_prerendering);
};

}  // namespace chrome

#endif  // CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
