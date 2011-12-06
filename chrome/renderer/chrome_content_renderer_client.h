// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#define CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "content/public/renderer/content_renderer_client.h"

class ChromeRenderProcessObserver;
class Extension;
class ExtensionDispatcher;
class ExtensionSet;
class RendererHistogramSnapshots;
class RendererNetPredictor;
class SpellCheck;
class SpellCheckProvider;
class VisitedLinkSlave;

namespace safe_browsing {
class PhishingClassifierFilter;
}

namespace webkit {
struct WebPluginInfo;
namespace npapi {
class PluginGroup;
}
}

namespace chrome {

class ChromeContentRendererClient : public content::ContentRendererClient {
 public:
  ChromeContentRendererClient();
  virtual ~ChromeContentRendererClient();

  virtual void RenderThreadStarted() OVERRIDE;
  virtual void RenderViewCreated(content::RenderView* render_view) OVERRIDE;
  virtual void SetNumberOfViews(int number_of_views) OVERRIDE;
  virtual SkBitmap* GetSadPluginBitmap() OVERRIDE;
  virtual std::string GetDefaultEncoding() OVERRIDE;
  virtual bool OverrideCreatePlugin(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      WebKit::WebPlugin** plugin) OVERRIDE;
  virtual bool HasErrorPage(int http_status_code,
                            std::string* error_domain) OVERRIDE;
  virtual void GetNavigationErrorStrings(
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error,
      std::string* error_html,
      string16* error_description) OVERRIDE;
  virtual bool RunIdleHandlerWhenWidgetsHidden() OVERRIDE;
  virtual bool AllowPopup(const GURL& creator) OVERRIDE;
  virtual bool ShouldFork(WebKit::WebFrame* frame,
                          const GURL& url,
                          bool is_content_initiated,
                          bool is_initial_navigation,
                          bool* send_referrer) OVERRIDE;
  virtual bool WillSendRequest(WebKit::WebFrame* frame,
                               const GURL& url,
                               GURL* new_url) OVERRIDE;
  virtual bool ShouldPumpEventsDuringCookieMessage() OVERRIDE;
  virtual void DidCreateScriptContext(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int world_id) OVERRIDE;
  virtual void WillReleaseScriptContext(WebKit::WebFrame* frame,
                                        v8::Handle<v8::Context> context,
                                        int world_id) OVERRIDE;
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length) OVERRIDE;
  virtual bool IsLinkVisited(unsigned long long link_hash) OVERRIDE;
  virtual void PrefetchHostName(const char* hostname, size_t length) OVERRIDE;
  virtual bool ShouldOverridePageVisibilityState(
      const content::RenderView* render_view,
      WebKit::WebPageVisibilityState* override_state) const OVERRIDE;
  virtual bool HandleGetCookieRequest(content::RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      std::string* cookies) OVERRIDE;
  virtual bool HandleSetCookieRequest(content::RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& value) OVERRIDE;

  // TODO(mpcomplete): remove after we collect histogram data.
  // http://crbug.com/100411
  bool IsAdblockInstalled();
  bool IsAdblockPlusInstalled();
  bool IsAdblockWithWebRequestInstalled();
  bool IsAdblockPlusWithWebRequestInstalled();
  bool IsOtherExtensionWithWebRequestInstalled();

  // For testing.
  void SetExtensionDispatcher(ExtensionDispatcher* extension_dispatcher);

  // Called in low-memory conditions to dump the memory used by the spellchecker
  // and start over.
  void OnPurgeMemory();

  virtual void RegisterPPAPIInterfaceFactories(
      webkit::ppapi::PpapiInterfaceFactoryManager* factory_manager) OVERRIDE;

  virtual bool AllowSocketAPI(const GURL& url) OVERRIDE;

 private:
  WebKit::WebPlugin* CreatePlugin(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

  // Returns the extension for the given URL.  Excludes extension objects for
  // bookmark apps, which do not use the app process model.
  const Extension* GetNonBookmarkAppExtension(const ExtensionSet* extensions,
                                              const GURL& url);

  // Returns true if the frame is navigating to an URL either into or out of an
  // extension app's extent.
  bool CrossesExtensionExtents(WebKit::WebFrame* frame,
                               const GURL& new_url,
                               bool is_initial_navigation);

  // Returns true if the NaCl plugin can be created. If it returns true, as a
  // side effect, it may add special attributes to params.
  bool IsNaClAllowed(const webkit::WebPluginInfo& plugin,
                     const GURL& url,
                     const std::string& actual_mime_type,
                     bool is_nacl_mime_type,
                     bool enable_nacl,
                     WebKit::WebPluginParams& params);

  scoped_ptr<ChromeRenderProcessObserver> chrome_observer_;
  scoped_ptr<ExtensionDispatcher> extension_dispatcher_;
  scoped_ptr<RendererHistogramSnapshots> histogram_snapshots_;
  scoped_ptr<RendererNetPredictor> net_predictor_;
  scoped_ptr<SpellCheck> spellcheck_;
  // The SpellCheckProvider is a RenderViewObserver, and handles its own
  // destruction.
  SpellCheckProvider* spellcheck_provider_;
  scoped_ptr<VisitedLinkSlave> visited_link_slave_;
  scoped_ptr<safe_browsing::PhishingClassifierFilter> phishing_classifier_;

  // Set of origins that can use TCP/UDP private APIs from NaCl.
  std::set<std::string> allowed_socket_origins_;
};

}  // namespace chrome

#endif  // CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
