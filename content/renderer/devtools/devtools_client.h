// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CLIENT_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsFrontendClient.h"

namespace WebKit {
class WebDevToolsFrontend;
class WebString;
}

namespace content {

class RenderViewImpl;

// Developer tools UI end of communication channel between the render process of
// the page being inspected and tools UI renderer process. All messages will
// go through browser process. On the side of the inspected page there's
// corresponding DevToolsAgent object.
// TODO(yurys): now the client is almost empty later it will delegate calls to
// code in glue
class DevToolsClient : public RenderViewObserver,
                       public WebKit::WebDevToolsFrontendClient {
 public:
  explicit DevToolsClient(RenderViewImpl* render_view);
  virtual ~DevToolsClient();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebDevToolsFrontendClient implementation.
  virtual void sendMessageToBackend(const WebKit::WebString&) OVERRIDE;

  virtual void activateWindow() OVERRIDE;
  virtual void closeWindow() OVERRIDE;
  virtual void moveWindowBy(const WebKit::WebFloatPoint& offset) OVERRIDE;
  virtual void requestSetDockSide(const WebKit::WebString& side) OVERRIDE;
  virtual void openInNewTab(const WebKit::WebString& side) OVERRIDE;
  virtual void save(const WebKit::WebString& url,
                    const WebKit::WebString& content,
                    bool save_as) OVERRIDE;
  virtual void append(const WebKit::WebString& url,
                      const WebKit::WebString& content) OVERRIDE;

  virtual void requestFileSystems() OVERRIDE;
  virtual void addFileSystem() OVERRIDE;
  virtual void removeFileSystem(
      const WebKit::WebString& fileSystemPath) OVERRIDE;

  void OnDispatchOnInspectorFrontend(const std::string& message);

  scoped_ptr<WebKit::WebDevToolsFrontend> web_tools_frontend_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CLIENT_H_
