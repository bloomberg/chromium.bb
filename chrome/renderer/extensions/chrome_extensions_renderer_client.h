// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "ui/base/page_transition_types.h"

class ChromeExtensionsDispatcherDelegate;
class GURL;

namespace blink {
class WebFrame;
class WebLocalFrame;
struct WebPluginParams;
}

namespace content {
class BrowserPluginDelegate;
class RenderFrame;
class RenderView;
}

namespace extensions {
class Dispatcher;
class ExtensionsGuestViewContainerDispatcher;
class RendererPermissionsPolicyDelegate;
class ResourceRequestPolicy;
}

class ChromeExtensionsRendererClient
    : public extensions::ExtensionsRendererClient {
 public:
  ChromeExtensionsRendererClient();
  ~ChromeExtensionsRendererClient() override;

  // Get the LazyInstance for ChromeExtensionsRendererClient.
  static ChromeExtensionsRendererClient* GetInstance();

  // extensions::ExtensionsRendererClient implementation.
  bool IsIncognitoProcess() const override;
  int GetLowestIsolatedWorldId() const override;

  // See ChromeContentRendererClient methods with the same names.
  void RenderThreadStarted();
  void RenderFrameCreated(content::RenderFrame* render_frame);
  void RenderViewCreated(content::RenderView* render_view);
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params);
  bool AllowPopup();
  bool WillSendRequest(blink::WebFrame* frame,
                       ui::PageTransition transition_type,
                       const GURL& url,
                       GURL* new_url);
  void SetExtensionDispatcherForTest(
      scoped_ptr<extensions::Dispatcher> extension_dispatcher);
  extensions::Dispatcher* GetExtensionDispatcherForTest();

  static bool ShouldFork(blink::WebLocalFrame* frame,
                         const GURL& url,
                         bool is_initial_navigation,
                         bool is_server_redirect,
                         bool* send_referrer);
  static content::BrowserPluginDelegate* CreateBrowserPluginDelegate(
      content::RenderFrame* render_frame,
      const std::string& mime_type,
      const GURL& original_url);

  extensions::Dispatcher* extension_dispatcher() {
    return extension_dispatcher_.get();
  }

 private:
  scoped_ptr<ChromeExtensionsDispatcherDelegate> extension_dispatcher_delegate_;
  scoped_ptr<extensions::Dispatcher> extension_dispatcher_;
  scoped_ptr<extensions::RendererPermissionsPolicyDelegate>
      permissions_policy_delegate_;
  scoped_ptr<extensions::ExtensionsGuestViewContainerDispatcher>
      guest_view_container_dispatcher_;
  scoped_ptr<extensions::ResourceRequestPolicy> resource_request_policy_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsRendererClient);
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
