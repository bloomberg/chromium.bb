// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#define CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_

#include <string>
#include <vector>

#if defined(ENABLE_PLUGINS)
#include <set>
#endif

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/public/renderer/content_renderer_client.h"

class ChromeRenderProcessObserver;
class ExtensionSet;
class PrescientNetworkingDispatcher;
class RendererNetPredictor;
#if defined(ENABLE_SPELLCHECK)
class SpellCheck;
class SpellCheckProvider;
#endif

struct ChromeViewHostMsg_GetPluginInfo_Output;

namespace content {
struct WebPluginInfo;
}

namespace extensions {
class Dispatcher;
class Extension;
class RendererPermissionsPolicyDelegate;
}

namespace prerender {
class PrerenderDispatcher;
}

namespace safe_browsing {
class PhishingClassifierFilter;
}

namespace visitedlink {
class VisitedLinkSlave;
}

namespace WebKit {
class WebSecurityOrigin;
}

#if defined(ENABLE_WEBRTC)
class WebRtcLoggingMessageFilter;
#endif

namespace chrome {

class ChromeContentRendererClient : public content::ContentRendererClient {
 public:
  ChromeContentRendererClient();
  virtual ~ChromeContentRendererClient();

  virtual void RenderThreadStarted() OVERRIDE;
  virtual void RenderViewCreated(content::RenderView* render_view) OVERRIDE;
  virtual void SetNumberOfViews(int number_of_views) OVERRIDE;
  virtual SkBitmap* GetSadPluginBitmap() OVERRIDE;
  virtual SkBitmap* GetSadWebViewBitmap() OVERRIDE;
  virtual std::string GetDefaultEncoding() OVERRIDE;
  virtual bool OverrideCreatePlugin(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      WebKit::WebPlugin** plugin) OVERRIDE;
  virtual WebKit::WebPlugin* CreatePluginReplacement(
      content::RenderView* render_view,
      const base::FilePath& plugin_path) OVERRIDE;
  virtual bool HasErrorPage(int http_status_code,
                            std::string* error_domain) OVERRIDE;
  virtual void GetNavigationErrorStrings(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error,
      std::string* error_html,
      string16* error_description) OVERRIDE;
  virtual void DeferMediaLoad(content::RenderView* render_view,
                              const base::Closure& closure) OVERRIDE;
  virtual bool RunIdleHandlerWhenWidgetsHidden() OVERRIDE;
  virtual bool AllowPopup() OVERRIDE;
  virtual bool ShouldFork(WebKit::WebFrame* frame,
                          const GURL& url,
                          const std::string& http_method,
                          bool is_initial_navigation,
                          bool is_server_redirect,
                          bool* send_referrer) OVERRIDE;
  virtual bool WillSendRequest(WebKit::WebFrame* frame,
                               content::PageTransition transition_type,
                               const GURL& url,
                               const GURL& first_party_for_cookies,
                               GURL* new_url) OVERRIDE;
  virtual bool ShouldPumpEventsDuringCookieMessage() OVERRIDE;
  virtual void DidCreateScriptContext(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id) OVERRIDE;
  virtual void WillReleaseScriptContext(WebKit::WebFrame* frame,
                                        v8::Handle<v8::Context> context,
                                        int world_id) OVERRIDE;
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length) OVERRIDE;
  virtual bool IsLinkVisited(unsigned long long link_hash) OVERRIDE;
  virtual WebKit::WebPrescientNetworking* GetPrescientNetworking() OVERRIDE;
  virtual bool ShouldOverridePageVisibilityState(
      const content::RenderView* render_view,
      WebKit::WebPageVisibilityState* override_state) OVERRIDE;
  virtual bool HandleGetCookieRequest(content::RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      std::string* cookies) OVERRIDE;
  virtual bool HandleSetCookieRequest(content::RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& value) OVERRIDE;
  virtual bool AllowBrowserPlugin(
      WebKit::WebPluginContainer* container) OVERRIDE;
  virtual const void* CreatePPAPIInterface(
      const std::string& interface_name) OVERRIDE;
  virtual bool IsExternalPepperPlugin(const std::string& module_name) OVERRIDE;
  // TODO(victorhsieh): move to ChromeContentBrowserClient once we migrate
  // PPAPI FileIO host to browser.
  virtual bool IsPluginAllowedToCallRequestOSFileHandle(
      WebKit::WebPluginContainer* container) OVERRIDE;
  virtual WebKit::WebSpeechSynthesizer* OverrideSpeechSynthesizer(
      WebKit::WebSpeechSynthesizerClient* client) OVERRIDE;
  virtual bool AllowPepperMediaStreamAPI(const GURL& url) OVERRIDE;

  // For testing.
  void SetExtensionDispatcher(extensions::Dispatcher* extension_dispatcher);

#if defined(ENABLE_SPELLCHECK)
  // Sets a new |spellcheck|. Used for low-mem restart and testing only.
  // Takes ownership of |spellcheck|.
  void SetSpellcheck(SpellCheck* spellcheck);
#endif

  // Called in low-memory conditions to dump the memory used by the spellchecker
  // and start over.
  void OnPurgeMemory();

  static WebKit::WebPlugin* CreatePlugin(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      const ChromeViewHostMsg_GetPluginInfo_Output& output);

  // TODO(mpcomplete): remove after we collect histogram data.
  // http://crbug.com/100411
  static bool IsAdblockInstalled();
  static bool IsAdblockPlusInstalled();
  static bool IsAdblockWithWebRequestInstalled();
  static bool IsAdblockPlusWithWebRequestInstalled();
  static bool IsOtherExtensionWithWebRequestInstalled();

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest, NaClRestriction);

  const extensions::Extension* GetExtension(
      const WebKit::WebSecurityOrigin& origin) const;

  // Returns true if the frame is navigating to an URL either into or out of an
  // extension app's extent.
  bool CrossesExtensionExtents(WebKit::WebFrame* frame,
                               const GURL& new_url,
                               const ExtensionSet& extensions,
                               bool is_extension_url,
                               bool is_initial_navigation);

  static GURL GetNaClContentHandlerURL(const std::string& actual_mime_type,
                                       const content::WebPluginInfo& plugin);
  static bool IsNaClAllowed(const GURL& manifest_url,
                            const GURL& app_url,
                            bool is_nacl_unrestricted,
                            const extensions::Extension* extension,
                            WebKit::WebPluginParams* params);

  scoped_ptr<ChromeRenderProcessObserver> chrome_observer_;
  scoped_ptr<extensions::Dispatcher> extension_dispatcher_;
  scoped_ptr<extensions::RendererPermissionsPolicyDelegate>
      permissions_policy_delegate_;
  scoped_ptr<PrescientNetworkingDispatcher> prescient_networking_dispatcher_;
  scoped_ptr<RendererNetPredictor> net_predictor_;
#if defined(ENABLE_SPELLCHECK)
  scoped_ptr<SpellCheck> spellcheck_;
#endif
  scoped_ptr<visitedlink::VisitedLinkSlave> visited_link_slave_;
  scoped_ptr<safe_browsing::PhishingClassifierFilter> phishing_classifier_;
  scoped_ptr<prerender::PrerenderDispatcher> prerender_dispatcher_;
#if defined(ENABLE_WEBRTC)
  scoped_refptr<WebRtcLoggingMessageFilter> webrtc_logging_message_filter_;
#endif

#if defined(ENABLE_PLUGINS)
  std::set<std::string> allowed_file_handle_origins_;
#endif
};

}  // namespace chrome

#endif  // CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
