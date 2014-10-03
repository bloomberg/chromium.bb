// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
#define EXTENSIONS_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/content_renderer_client.h"

namespace extensions {

class Dispatcher;
class DispatcherDelegate;
class ShellExtensionsClient;
class ShellExtensionsRendererClient;
class ShellRendererMainDelegate;

// Renderer initialization and runtime support for app_shell.
class ShellContentRendererClient : public content::ContentRendererClient {
 public:
  ShellContentRendererClient();
  virtual ~ShellContentRendererClient();

  // content::ContentRendererClient implementation:
  virtual void RenderThreadStarted() override;
  virtual void RenderFrameCreated(content::RenderFrame* render_frame) override;
  virtual void RenderViewCreated(content::RenderView* render_view) override;
  virtual bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                                    blink::WebLocalFrame* frame,
                                    const blink::WebPluginParams& params,
                                    blink::WebPlugin** plugin) override;
  virtual blink::WebPlugin* CreatePluginReplacement(
      content::RenderFrame* render_frame,
      const base::FilePath& plugin_path) override;
  virtual bool WillSendRequest(blink::WebFrame* frame,
                               ui::PageTransition transition_type,
                               const GURL& url,
                               const GURL& first_party_for_cookies,
                               GURL* new_url) override;
  virtual void DidCreateScriptContext(blink::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id) override;
  virtual const void* CreatePPAPIInterface(
      const std::string& interface_name) override;
  virtual bool IsExternalPepperPlugin(const std::string& module_name) override;
  virtual bool ShouldEnableSiteIsolationPolicy() const override;
  virtual content::BrowserPluginDelegate* CreateBrowserPluginDelegate(
      content::RenderFrame* render_frame,
      const std::string& mime_type) override;

 private:
  scoped_ptr<ShellExtensionsClient> extensions_client_;
  scoped_ptr<ShellExtensionsRendererClient> extensions_renderer_client_;
  scoped_ptr<DispatcherDelegate> extension_dispatcher_delegate_;
  scoped_ptr<Dispatcher> extension_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ShellContentRendererClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
