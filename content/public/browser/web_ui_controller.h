// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "content/common/content_export.h"

class GURL;
class RenderViewHost;
class WebUI;

namespace base {
class ListValue;
}

namespace content {

// A WebUI page is controller by the embedder's WebUIController object. It
// manages the data source and message handlers.
class CONTENT_EXPORT WebUIController {
 public:
  explicit WebUIController(WebUI* web_ui) : web_ui_(web_ui) {}
  virtual ~WebUIController() {}

  // Allows the controller to override handling all messages from the page.
  // Return true if the message handling was overridden.
  virtual bool OverrideHandleWebUIMessage(const GURL& source_url,
                                          const std::string& message,
                                          const base::ListValue& args);

  // Called when RenderView is first created. This is *not* called for every
  // page load because in some cases a RenderView will be reused. In those
  // cases, RenderViewReused will be called instead.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // Called when a RenderView is reused to display a page.
  virtual void RenderViewReused(RenderViewHost* render_view_host) {}

  // Called when this becomes the active WebUI instance for a re-used
  // RenderView; this is the point at which this WebUI instance will receive
  // DOM messages instead of the previous WebUI instance.
  //
  // If a WebUI instance has code that is usually triggered from a JavaScript
  // onload handler, this should be overridden to check to see if the web page's
  // DOM is still intact (e.g., due to a back/forward navigation that remains
  // within the same page), and if so trigger that code manually since onload
  // won't be run in that case.
  virtual void DidBecomeActiveForReusedRenderView() {}

  WebUI* web_ui() const { return web_ui_; }

 private:
  WebUI* web_ui_;

};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_CONTROLLER_H_
