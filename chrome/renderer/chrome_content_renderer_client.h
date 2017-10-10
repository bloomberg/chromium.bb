// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#define CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "chrome/renderer/media/chrome_key_systems_provider.h"
#include "components/nacl/common/features.h"
#include "components/rappor/public/interfaces/rappor_recorder.mojom.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "content/public/renderer/content_renderer_client.h"
#include "extensions/features/features.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"
#include "printing/features/features.h"
#include "v8/include/v8.h"

#if defined (OS_CHROMEOS)
#include "chrome/renderer/leak_detector/leak_detector_remote_client.h"
#endif

#if defined(OS_WIN)
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#endif

class ChromeRenderThreadObserver;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class ChromePDFPrintClient;
#endif
class PrescientNetworkingDispatcher;
#if BUILDFLAG(ENABLE_SPELLCHECK)
class SpellCheck;
#endif

struct ChromeViewHostMsg_GetPluginInfo_Output;

namespace content {
class BrowserPluginDelegate;
struct WebPluginInfo;
}

namespace error_page {
class Error;
}

namespace network_hints {
class PrescientNetworkingDispatcher;
}

namespace extensions {
class Extension;
}

namespace prerender {
class PrerenderDispatcher;
}

namespace safe_browsing {
class PhishingClassifierFilter;
}

namespace subresource_filter {
class UnverifiedRulesetDealer;
}

namespace web_cache {
class WebCacheImpl;
}

#if BUILDFLAG(ENABLE_WEBRTC)
class WebRtcLoggingMessageFilter;
#endif

namespace internal {

extern const char kFlashYouTubeRewriteUMA[];

// Used for UMA. Values should not be reorderer or reused.
// SUCCESS refers to an embed properly rewritten. SUCCESS_PARAMS_REWRITE refers
// to an embed rewritten with the params fixed. SUCCESS_ENABLEJSAPI refers to
// a rewritten embed even though the JS API was enabled (Chrome Android only).
// FAILURE_ENABLEJSAPI indicates the embed was not rewritten because the
// JS API was enabled.
enum YouTubeRewriteStatus {
  SUCCESS = 0,
  SUCCESS_PARAMS_REWRITE = 1,
  SUCCESS_ENABLEJSAPI = 2,
  FAILURE_ENABLEJSAPI = 3,
  NUM_PLUGIN_ERROR  // should be kept last
};

}  // namespace internal

class ChromeContentRendererClient : public content::ContentRendererClient {
 public:
  ChromeContentRendererClient();
  ~ChromeContentRendererClient() override;

  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RenderViewCreated(content::RenderView* render_view) override;
  SkBitmap* GetSadPluginBitmap() override;
  SkBitmap* GetSadWebViewBitmap() override;
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params,
                            blink::WebPlugin** plugin) override;
  blink::WebPlugin* CreatePluginReplacement(
      content::RenderFrame* render_frame,
      const base::FilePath& plugin_path) override;
  bool HasErrorPage(int http_status_code) override;
  bool ShouldSuppressErrorPage(content::RenderFrame* render_frame,
                               const GURL& url) override;
  void GetNavigationErrorStrings(content::RenderFrame* render_frame,
                                 const blink::WebURLRequest& failed_request,
                                 const blink::WebURLError& error,
                                 std::string* error_html,
                                 base::string16* error_description) override;
  void GetNavigationErrorStringsForHttpStatusError(
      content::RenderFrame* render_frame,
      const blink::WebURLRequest& failed_request,
      const GURL& unreachable_url,
      int http_status,
      std::string* error_html,
      base::string16* error_description) override;

  void DeferMediaLoad(content::RenderFrame* render_frame,
                      bool has_played_media_before,
                      const base::Closure& closure) override;
  bool RunIdleHandlerWhenWidgetsHidden() override;
  bool AllowStoppingWhenProcessBackgrounded() override;
  bool AllowPopup() override;
  bool ShouldFork(blink::WebLocalFrame* frame,
                  const GURL& url,
                  const std::string& http_method,
                  bool is_initial_navigation,
                  bool is_server_redirect,
                  bool* send_referrer) override;
  bool WillSendRequest(
      blink::WebLocalFrame* frame,
      ui::PageTransition transition_type,
      const blink::WebURL& url,
      std::vector<std::unique_ptr<content::URLLoaderThrottle>>* throttles,
      GURL* new_url) override;
  bool IsPrefetchOnly(content::RenderFrame* render_frame,
                      const blink::WebURLRequest& request) override;
  unsigned long long VisitedLinkHash(const char* canonical_url,
                                     size_t length) override;
  bool IsLinkVisited(unsigned long long link_hash) override;
  blink::WebPrescientNetworking* GetPrescientNetworking() override;
  bool ShouldOverridePageVisibilityState(
      const content::RenderFrame* render_frame,
      blink::WebPageVisibilityState* override_state) override;
  bool IsExternalPepperPlugin(const std::string& module_name) override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle() override;
  std::unique_ptr<blink::WebSpeechSynthesizer> OverrideSpeechSynthesizer(
      blink::WebSpeechSynthesizerClient* client) override;
  bool ShouldReportDetailedMessageForSource(
      const base::string16& source) const override;
  bool ShouldGatherSiteIsolationStats() const override;
  std::unique_ptr<blink::WebContentSettingsClient>
  CreateWorkerContentSettingsClient(
      content::RenderFrame* render_frame) override;
  bool AllowPepperMediaStreamAPI(const GURL& url) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems)
      override;
  bool IsKeySystemsUpdateNeeded() override;
  bool IsPluginAllowedToUseDevChannelAPIs() override;
  bool IsPluginAllowedToUseCameraDeviceAPI(const GURL& url) override;
  bool IsPluginAllowedToUseCompositorAPI(const GURL& url) override;
  content::BrowserPluginDelegate* CreateBrowserPluginDelegate(
      content::RenderFrame* render_frame,
      const std::string& mime_type,
      const GURL& original_url) override;
  void RecordRappor(const std::string& metric,
                    const std::string& sample) override;
  void RecordRapporURL(const std::string& metric, const GURL& url) override;
  void AddImageContextMenuProperties(
      const blink::WebURLResponse& response,
      bool is_image_in_context_a_placeholder_image,
      std::map<std::string, std::string>* properties) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame) override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  void DidInitializeServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) override;
  void WillDestroyServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) override;
  bool ShouldEnforceWebRTCRoutingPreferences() override;
  GURL OverrideFlashEmbedWithHTML(const GURL& url) override;
  std::unique_ptr<base::TaskScheduler::InitParams> GetTaskSchedulerInitParams()
      override;
  bool OverrideLegacySymantecCertConsoleMessage(
      const GURL& url,
      base::Time cert_validity_start,
      std::string* console_messsage) override;

#if BUILDFLAG(ENABLE_SPELLCHECK)
  // Sets a new |spellcheck|. Used for testing only.
  // Takes ownership of |spellcheck|.
  void SetSpellcheck(SpellCheck* spellcheck);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  static blink::WebPlugin* CreatePlugin(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params,
      const ChromeViewHostMsg_GetPluginInfo_Output& output);
#endif

#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  static bool IsExtensionOrSharedModuleWhitelisted(
      const GURL& url, const std::set<std::string>& whitelist);
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest, NaClRestriction);
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest,
                           ShouldSuppressErrorPage);

  static GURL GetNaClContentHandlerURL(const std::string& actual_mime_type,
                                       const content::WebPluginInfo& plugin);

  // Initialises |safe_browsing_| if it is not already initialised.
  void InitSafeBrowsingIfNecessary();

  void GetNavigationErrorStringsInternal(
      content::RenderFrame* render_frame,
      const blink::WebURLRequest& failed_request,
      const error_page::Error& error,
      std::string* error_html,
      base::string16* error_description);

  // Time at which this object was created. This is very close to the time at
  // which the RendererMain function was entered.
  base::TimeTicks main_entry_time_;

#if BUILDFLAG(ENABLE_NACL)
  // Determines if a NaCl app is allowed, and modifies params to pass the app's
  // permissions to the trusted NaCl plugin.
  static bool IsNaClAllowed(const GURL& manifest_url,
                            const GURL& app_url,
                            bool is_nacl_unrestricted,
                            const extensions::Extension* extension,
                            blink::WebPluginParams* params);
#endif

  rappor::mojom::RapporRecorderPtr rappor_recorder_;

  std::unique_ptr<ChromeRenderThreadObserver> chrome_observer_;
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;

  std::unique_ptr<network_hints::PrescientNetworkingDispatcher>
      prescient_networking_dispatcher_;

  chrome::ChromeKeySystemsProvider key_systems_provider_;

  safe_browsing::mojom::SafeBrowsingPtr safe_browsing_;

#if BUILDFLAG(ENABLE_SPELLCHECK)
  std::unique_ptr<SpellCheck> spellcheck_;
#endif
  std::unique_ptr<safe_browsing::PhishingClassifierFilter> phishing_classifier_;
  std::unique_ptr<subresource_filter::UnverifiedRulesetDealer>
      subresource_filter_ruleset_dealer_;
  std::unique_ptr<prerender::PrerenderDispatcher> prerender_dispatcher_;
#if BUILDFLAG(ENABLE_WEBRTC)
  scoped_refptr<WebRtcLoggingMessageFilter> webrtc_logging_message_filter_;
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  std::unique_ptr<ChromePDFPrintClient> pdf_print_client_;
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
  std::set<std::string> allowed_camera_device_origins_;
  std::set<std::string> allowed_compositor_origins_;
#endif

#if defined(OS_CHROMEOS)
  std::unique_ptr<LeakDetectorRemoteClient> leak_detector_remote_client_;
#endif

#if defined(OS_WIN)
  // Observes module load and unload events and notifies the ModuleDatabase in
  // the browser process.
  std::unique_ptr<ModuleWatcher> module_watcher_;
  mojom::ModuleEventSinkPtr module_event_sink_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeContentRendererClient);
};

#endif  // CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
