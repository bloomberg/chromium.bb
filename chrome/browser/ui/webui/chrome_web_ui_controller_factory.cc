// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/bookmarks_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/browser/ui/webui/downloads_ui.h"
#include "chrome/browser/ui/webui/extensions/extension_activity_ui.h"
#include "chrome/browser/ui/webui/extensions/extension_info_ui.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "chrome/browser/ui/webui/feedback_ui.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/browser/ui/webui/flash_ui.h"
#include "chrome/browser/ui/webui/generic_handler.h"
#include "chrome/browser/ui/webui/gpu_internals_ui.h"
#include "chrome/browser/ui/webui/help/help_ui.h"
#include "chrome/browser/ui/webui/history_ui.h"
#include "chrome/browser/ui/webui/inspect_ui.h"
#include "chrome/browser/ui/webui/instant_ui.h"
#include "chrome/browser/ui/webui/local_omnibox_popup/local_omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/media/media_internals_ui.h"
#if !defined(DISABLE_NACL)
#include "chrome/browser/ui/webui/nacl_ui.h"
#endif
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui.h"
#include "chrome/browser/ui/webui/plugins_ui.h"
#include "chrome/browser/ui/webui/predictors/predictors_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/profiler_ui.h"
#include "chrome/browser/ui/webui/quota_internals_ui.h"
#include "chrome/browser/ui/webui/signin_internals_ui.h"
#include "chrome/browser/ui/webui/sync_internals_ui.h"
#include "chrome/browser/ui/webui/task_manager/task_manager_ui.h"
#include "chrome/browser/ui/webui/test_chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/tracing_ui.h"
#include "chrome/browser/ui/webui/user_actions/user_actions_ui.h"
#include "chrome/browser/ui/webui/version_ui.h"
#include "chrome/browser/ui/webui/webrtc_internals_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "extensions/common/constants.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/favicon_size.h"
#include "ui/web_dialogs/web_dialog_ui.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/ui/webui/policy_ui.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/welcome_ui_android.h"
#else
#include "chrome/browser/ui/webui/suggestions_internals/suggestions_internals_ui.h"
#include "chrome/browser/ui/webui/uber/uber_ui.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/choose_mobile_network_ui.h"
#include "chrome/browser/ui/webui/chromeos/cryptohome_ui.h"
#include "chrome/browser/ui/webui/chromeos/diagnostics/diagnostics_ui.h"
#include "chrome/browser/ui/webui/chromeos/drive_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/imageburner/imageburner_ui.h"
#include "chrome/browser/ui/webui/chromeos/keyboard_overlay_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_ui.h"
#include "chrome/browser/ui/webui/chromeos/proxy_settings_ui.h"
#include "chrome/browser/ui/webui/chromeos/register_page_ui.h"
#include "chrome/browser/ui/webui/chromeos/sim_unlock_ui.h"
#include "chrome/browser/ui/webui/chromeos/system_info_ui.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/webui/gesture_config_ui.h"
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/webui/conflicts_ui.h"
#include "chrome/browser/ui/webui/set_as_default_browser_ui.h"
#endif

#if (defined(USE_NSS) || defined(USE_OPENSSL)) && defined(USE_AURA)
#include "chrome/browser/ui/webui/certificate_viewer_ui.h"
#endif

using content::WebUI;
using content::WebUIController;
using ui::ExternalWebDialogUI;
using ui::WebDialogUI;

namespace {

// A function for creating a new WebUI. The caller owns the return value, which
// may be NULL (for example, if the URL refers to an non-existent extension).
typedef WebUIController* (*WebUIFactoryFunction)(WebUI* web_ui,
                                                 const GURL& url);

// Template for defining WebUIFactoryFunction.
template<class T>
WebUIController* NewWebUI(WebUI* web_ui, const GURL& url) {
  return new T(web_ui);
}

// Special cases for extensions.
template<>
WebUIController* NewWebUI<ExtensionWebUI>(WebUI* web_ui,
                                          const GURL& url) {
  return new ExtensionWebUI(web_ui, url);
}

template<>
WebUIController* NewWebUI<ExtensionInfoUI>(WebUI* web_ui,
                                           const GURL& url) {
  return new ExtensionInfoUI(web_ui, url);
}

// Special case for older about: handlers.
template<>
WebUIController* NewWebUI<AboutUI>(WebUI* web_ui, const GURL& url) {
  return new AboutUI(web_ui, url.host());
}

// Only create ExtensionWebUI for URLs that are allowed extension bindings,
// hosted by actual tabs.
bool NeedsExtensionWebUI(Profile* profile, const GURL& url) {
  ExtensionService* service = profile ? profile->GetExtensionService() : NULL;
  return service && service->ExtensionBindingsAllowed(url);
}

// Returns a function that can be used to create the right type of WebUI for a
// tab, based on its URL. Returns NULL if the URL doesn't have WebUI associated
// with it.
WebUIFactoryFunction GetWebUIFactoryFunction(WebUI* web_ui,
                                             Profile* profile,
                                             const GURL& url) {
#if defined(ENABLE_EXTENSIONS)
  if (NeedsExtensionWebUI(profile, url))
    return &NewWebUI<ExtensionWebUI>;
#endif

  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
  if (!url.SchemeIs(chrome::kChromeDevToolsScheme) &&
      !url.SchemeIs(chrome::kChromeInternalScheme) &&
      !url.SchemeIs(chrome::kChromeUIScheme)) {
    return NULL;
  }

  // Special case the new tab page. In older versions of Chrome, the new tab
  // page was hosted at chrome-internal:<blah>. This might be in people's saved
  // sessions or bookmarks, so we say any URL with that scheme triggers the new
  // tab page.
  if (url.host() == chrome::kChromeUINewTabHost ||
      url.SchemeIs(chrome::kChromeInternalScheme)) {
    return &NewWebUI<NewTabUI>;
  }

  /****************************************************************************
   * Please keep this in alphabetical order. If #ifs or special logics are
   * required, add it below in the appropriate section.
   ***************************************************************************/
  // We must compare hosts only since some of the Web UIs append extra stuff
  // after the host name.
  // All platform builds of Chrome will need to have a cloud printing
  // dialog as backup.  It's just that on Chrome OS, it's the only
  // print dialog.
  if (url.host() == chrome::kChromeUICloudPrintResourcesHost)
    return &NewWebUI<ExternalWebDialogUI>;
  if (url.host() == chrome::kChromeUICloudPrintSetupHost)
    return &NewWebUI<WebDialogUI>;
  if (url.spec() == chrome::kChromeUIConstrainedHTMLTestURL)
    return &NewWebUI<ConstrainedWebDialogUI>;
  if (url.host() == chrome::kChromeUIGpuInternalsHost)
    return &NewWebUI<GpuInternalsUI>;
  if (url.host() == chrome::kChromeUIHistoryFrameHost)
    return &NewWebUI<HistoryUI>;
  if (url.host() == chrome::kChromeUIInstantHost)
    return &NewWebUI<InstantUI>;
  if (url.host() == chrome::kChromeUILocalOmniboxPopupHost)
    return &NewWebUI<LocalOmniboxPopupUI>;
  if (url.host() == chrome::kChromeUIMediaInternalsHost)
    return &NewWebUI<MediaInternalsUI>;
#if !defined(DISABLE_NACL)
  if (url.host() == chrome::kChromeUINaClHost)
    return &NewWebUI<NaClUI>;
#endif
  if (url.host() == chrome::kChromeUINetInternalsHost)
    return &NewWebUI<NetInternalsUI>;
  if (url.host() == chrome::kChromeUIOmniboxHost)
    return &NewWebUI<OmniboxUI>;
  if (url.host() == chrome::kChromeUIPredictorsHost)
    return &NewWebUI<PredictorsUI>;
  if (url.host() == chrome::kChromeUIProfilerHost)
    return &NewWebUI<ProfilerUI>;
  if (url.host() == chrome::kChromeUIQuotaInternalsHost)
    return &NewWebUI<QuotaInternalsUI>;
  if (url.host() == chrome::kChromeUISignInInternalsHost)
    return &NewWebUI<SignInInternalsUI>;
  if (url.host() == chrome::kChromeUISyncInternalsHost)
    return &NewWebUI<SyncInternalsUI>;
  if (url.host() == chrome::kChromeUISyncResourcesHost)
    return &NewWebUI<WebDialogUI>;
  if (url.host() == chrome::kChromeUIUserActionsHost)
    return &NewWebUI<UserActionsUI>;
  if (url.host() == chrome::kChromeUIVersionHost)
    return &NewWebUI<VersionUI>;
  if (url.host() == chrome::kChromeUIWebRTCInternalsHost)
    return &NewWebUI<WebRTCInternalsUI>;

  /****************************************************************************
   * OS Specific #defines
   ***************************************************************************/
#if defined(OS_ANDROID)
  if (url.host() == chrome::kChromeUIWelcomeHost)
    return &NewWebUI<WelcomeUI>;
#else
  // Bookmarks are part of NTP on Android.
  if (url.host() == chrome::kChromeUIBookmarksHost)
    return &NewWebUI<BookmarksUI>;
  // Crashes page not supported on Android for now.
  if (url.host() == chrome::kChromeUICrashesHost)
    return &NewWebUI<CrashesUI>;
  if (url.host() == chrome::kChromeUIDevToolsHost)
    return &NewWebUI<DevToolsUI>;
  // Downloads list on Android uses the built-in download manager.
  if (url.host() == chrome::kChromeUIDownloadsHost)
    return &NewWebUI<DownloadsUI>;
  if (url.host() == chrome::kChromeUIDialogHost)
    return &NewWebUI<ConstrainedWebDialogUI>;
  // Feedback on Android uses the built-in feedback app.
  if (url.host() == chrome::kChromeUIFeedbackHost)
    return &NewWebUI<FeedbackUI>;
  // chrome://flags is currently unsupported on Android.
  // TODO(satish): Enable after http://crbug.com/143146 is fixed.
  if (url.host() == chrome::kChromeUIFlagsHost)
    return &NewWebUI<FlagsUI>;
  // Flash is not available on android.
  if (url.host() == chrome::kChromeUIFlashHost)
    return &NewWebUI<FlashUI>;
  // Help is implemented with native UI elements on Android.
  if (url.host() == chrome::kChromeUIHelpFrameHost)
    return &NewWebUI<HelpUI>;
  // chrome://inspect isn't supported on Android. Page debugging is handled by a
  // remote devtools on the host machine, and other elements (Shared Workers,
  // extensions, etc) aren't supported.
  if (url.host() == chrome::kChromeUIInspectHost)
    return &NewWebUI<InspectUI>;
  // Performance monitoring page is not on Android for now.
  if (url.host() == chrome::kChromeUIPerformanceMonitorHost)
    return &NewWebUI<performance_monitor::PerformanceMonitorUI>;
  // Android does not support plugins for now.
  if (url.host() == chrome::kChromeUIPluginsHost)
    return &NewWebUI<PluginsUI>;
  // Settings are implemented with native UI elements on Android.
  if (url.host() == chrome::kChromeUISettingsFrameHost)
    return &NewWebUI<options::OptionsUI>;
  if (url.host() == chrome::kChromeUISuggestionsInternalsHost)
    return &NewWebUI<SuggestionsInternalsUI>;
  if (url.host() == chrome::kChromeUITracingHost)
    return &NewWebUI<TracingUI>;
  // Uber frame is not used on Android.
  if (url.host() == chrome::kChromeUIUberFrameHost)
    return &NewWebUI<UberFrameUI>;
  // Uber page is not used on Android.
  if (url.host() == chrome::kChromeUIUberHost)
    return &NewWebUI<UberUI>;
#endif
#if defined(OS_WIN)
  if (url.host() == chrome::kChromeUIConflictsHost)
    return &NewWebUI<ConflictsUI>;
  if (url.host() == chrome::kChromeUIMetroFlowHost)
    return &NewWebUI<SetAsDefaultBrowserUI>;
#endif
#if (defined(USE_NSS) || defined(USE_OPENSSL)) && defined(USE_AURA)
  if (url.host() == chrome::kChromeUICertificateViewerHost)
    return &NewWebUI<CertificateViewerUI>;
#endif
#if defined(OS_CHROMEOS)
  if (url.host() == chrome::kChromeUIChooseMobileNetworkHost)
    return &NewWebUI<chromeos::ChooseMobileNetworkUI>;
  if (url.host() == chrome::kChromeUICryptohomeHost)
    return &NewWebUI<chromeos::CryptohomeUI>;
  if (url.host() == chrome::kChromeUIDriveInternalsHost)
    return &NewWebUI<chromeos::DriveInternalsUI>;
  if (url.host() == chrome::kChromeUIDiagnosticsHost)
    return &NewWebUI<chromeos::DiagnosticsUI>;
  if (url.host() == chrome::kChromeUIImageBurnerHost)
    return &NewWebUI<ImageBurnUI>;
  if (url.host() == chrome::kChromeUIKeyboardOverlayHost)
    return &NewWebUI<KeyboardOverlayUI>;
  if (url.host() == chrome::kChromeUIMobileSetupHost)
    return &NewWebUI<MobileSetupUI>;
  if (url.host() == chrome::kChromeUIOobeHost)
    return &NewWebUI<chromeos::OobeUI>;
  if (url.host() == chrome::kChromeUIProxySettingsHost)
    return &NewWebUI<chromeos::ProxySettingsUI>;
  if (url.host() == chrome::kChromeUIRegisterPageHost)
    return &NewWebUI<RegisterPageUI>;
  if (url.host() == chrome::kChromeUISimUnlockHost)
    return &NewWebUI<chromeos::SimUnlockUI>;
  if (url.host() == chrome::kChromeUISystemInfoHost)
    return &NewWebUI<chromeos::SystemInfoUI>;
#endif  // defined(OS_CHROMEOS)

  /****************************************************************************
   * Other #defines and special logics.
   ***************************************************************************/
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (url.host() == chrome::kChromeUIPolicyHost)
    return &NewWebUI<PolicyUI>;
#endif
#if defined(USE_AURA)
  if (url.host() == chrome::kChromeUITaskManagerHost)
    return &NewWebUI<TaskManagerUI>;
#endif  // defined(USE_AURA)

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
  if (url.host() == chrome::kChromeUICollectedCookiesHost ||
      url.host() == chrome::kChromeUIHttpAuthHost ||
      url.host() == chrome::kChromeUITabModalConfirmDialogHost) {
    return &NewWebUI<ConstrainedWebDialogUI>;
  }
#endif

#if defined(USE_AURA)
  if (url.host() == chrome::kChromeUIGestureConfigHost)
    return &NewWebUI<GestureConfigUI>;
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  if (url.host() == chrome::kChromeUISyncPromoHost) {
    // If the sync promo page is enabled then use the sync promo WebUI otherwise
    // use the NTP WebUI. We don't want to return NULL if the sync promo page
    // is disabled because the page can be disabled mid-flight (for example,
    // if sync login finishes).
    if (SyncPromoUI::ShouldShowSyncPromo(profile))
      return &NewWebUI<SyncPromoUI>;
    else
      return &NewWebUI<NewTabUI>;
  }
#endif

  if (url.host() == chrome::kChromeUIChromeURLsHost ||
      url.host() == chrome::kChromeUICreditsHost ||
      url.host() == chrome::kChromeUIDNSHost ||
      url.host() == chrome::kChromeUIMemoryHost ||
      url.host() == chrome::kChromeUIMemoryRedirectHost ||
      url.host() == chrome::kChromeUIStatsHost ||
      url.host() == chrome::kChromeUITermsHost
#if defined(OS_LINUX) || defined(OS_OPENBSD)
      || url.host() == chrome::kChromeUILinuxProxyConfigHost
      || url.host() == chrome::kChromeUISandboxHost
#endif
#if defined(OS_CHROMEOS)
      || url.host() == chrome::kChromeUIDiscardsHost
      || url.host() == chrome::kChromeUINetworkHost
      || url.host() == chrome::kChromeUIOSCreditsHost
      || url.host() == chrome::kChromeUITransparencyHost
#endif
#if defined(WEBUI_TASK_MANAGER)
      || url.host() == chrome::kChromeUITaskManagerHost
#endif
      ) {
    return &NewWebUI<AboutUI>;
  }

#if defined(ENABLE_EXTENSIONS)
  if (url.host() == chrome::kChromeUIExtensionActivityHost &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExtensionActivityUI)) {
    return &NewWebUI<ExtensionActivityUI>;
  }
  if (url.host() == chrome::kChromeUIExtensionInfoHost &&
      extensions::FeatureSwitch::script_badges()->IsEnabled()) {
    return &NewWebUI<ExtensionInfoUI>;
  }
  if (url.host() == chrome::kChromeUIExtensionsFrameHost)
    return &NewWebUI<ExtensionsUI>;
#endif
#if defined(ENABLE_PRINTING)
  if (url.host() == chrome::kChromeUIPrintHost &&
      !profile->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled))
    return &NewWebUI<PrintPreviewUI>;
#endif

  return NULL;
}

// When the test-type switch is set, return a TestType object, which should be a
// subclass of Type. The logic is provided here in the traits class, rather than
// in GetInstance() so that the choice is made only once, when the Singleton is
// first instantiated, rather than every time GetInstance() is called.
template<typename Type, typename TestType>
struct PossibleTestSingletonTraits : public DefaultSingletonTraits<Type> {
  static Type* New() {
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
      return DefaultSingletonTraits<TestType>::New();
    else
      return DefaultSingletonTraits<Type>::New();
  }
};

void RunFaviconCallbackAsync(
    const FaviconService::FaviconResultsCallback& callback,
    const std::vector<history::FaviconBitmapResult>* results,
    const history::IconURLSizesMap* size_map) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&FaviconService::FaviconResultsCallbackRunner,
                 callback, base::Owned(results), base::Owned(size_map)));
}

}  // namespace

WebUI::TypeID ChromeWebUIControllerFactory::GetWebUIType(
      content::BrowserContext* browser_context, const GURL& url) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebUIFactoryFunction function = GetWebUIFactoryFunction(NULL, profile, url);
  return function ? reinterpret_cast<WebUI::TypeID>(function) : WebUI::kNoWebUI;
}

bool ChromeWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context, const GURL& url) const {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

bool ChromeWebUIControllerFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context, const GURL& url) const {
  // Extensions are rendered via WebUI in tabs, but don't actually need WebUI
  // bindings (see the ExtensionWebUI constructor).
  return
      !NeedsExtensionWebUI(Profile::FromBrowserContext(browser_context), url) &&
      UseWebUIForURL(browser_context, url);
}

bool ChromeWebUIControllerFactory::IsURLAcceptableForWebUI(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool data_urls_allowed) const {
  return UseWebUIForURL(browser_context, url) ||
      // javacsript: URLs are allowed to run in Web UI pages
      url.SchemeIs(chrome::kJavaScriptScheme) ||
      // It's possible to load about:blank in a Web UI renderer.
      // See http://crbug.com/42547
      url.spec() == chrome::kAboutBlankURL ||
      // Chrome URLs crash, kill, hang, and shorthang are allowed.
      url == GURL(chrome::kChromeUICrashURL) ||
      url == GURL(chrome::kChromeUIKillURL) ||
      url == GURL(chrome::kChromeUIHangURL) ||
      url == GURL(content::kChromeUIShorthangURL) ||
      // Data URLs are usually not allowed in WebUI for security reasons.
      // BalloonHosts are one exception needed by ChromeOS, and are safe because
      // they cannot be scripted by other pages.
      (data_urls_allowed && url.SchemeIs(chrome::kDataScheme));
}

WebUIController* ChromeWebUIControllerFactory::CreateWebUIControllerForURL(
    WebUI* web_ui,
    const GURL& url) const {
  Profile* profile = Profile::FromWebUI(web_ui);
  WebUIFactoryFunction function = GetWebUIFactoryFunction(web_ui, profile, url);
  if (!function)
    return NULL;

  WebUIController* controller = (*function)(web_ui, url);
  if (controller)
    web_ui->AddMessageHandler(new GenericHandler());
  return controller;
}

void ChromeWebUIControllerFactory::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    const std::vector<ui::ScaleFactor>& scale_factors,
    const FaviconService::FaviconResultsCallback& callback) const {
  // Before determining whether page_url is an extension url, we must handle
  // overrides. This changes urls in |kChromeUIScheme| to extension urls, and
  // allows to use ExtensionWebUI::GetFaviconForURL.
  GURL url(page_url);
  ExtensionWebUI::HandleChromeURLOverride(&url, profile);

  // All extensions but the bookmark manager get their favicon from the icons
  // part of the manifest.
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      url.host() != extension_misc::kBookmarkManagerId) {
#if defined(ENABLE_EXTENSIONS)
    ExtensionWebUI::GetFaviconForURL(profile, url, callback);
#else
    RunFaviconCallbackAsync(callback,
                            new std::vector<history::FaviconBitmapResult>(),
                            new history::IconURLSizesMap());
#endif
    return;
  }

  std::vector<history::FaviconBitmapResult>* favicon_bitmap_results =
      new std::vector<history::FaviconBitmapResult>();
  history::IconURLSizesMap* icon_url_sizes = new history::IconURLSizesMap();

  for (size_t i = 0; i < scale_factors.size(); ++i) {
    scoped_refptr<base::RefCountedMemory> bitmap(GetFaviconResourceBytes(
          url, scale_factors[i]));
    if (bitmap.get() && bitmap->size()) {
      history::FaviconBitmapResult bitmap_result;
      bitmap_result.bitmap_data = bitmap;
      // Leave |bitmap_result|'s icon URL as the default of GURL().
      bitmap_result.icon_type = history::FAVICON;
      favicon_bitmap_results->push_back(bitmap_result);

      // Assume that |bitmap| is |gfx::kFaviconSize| x |gfx::kFaviconSize|
      // DIP.
      float scale = ui::GetScaleFactorScale(scale_factors[i]);
      int edge_pixel_size =
          static_cast<int>(gfx::kFaviconSize * scale + 0.5f);
      bitmap_result.pixel_size = gfx::Size(edge_pixel_size, edge_pixel_size);
    }
  }

  // Populate IconURLSizesMap such that the requirement that all the icon URLs
  // in |favicon_bitmap_results| be present in |icon_url_sizes| holds.
  // Populate the favicon sizes with the pixel sizes of the bitmaps available
  // for |url|.
  for (size_t i = 0; i < favicon_bitmap_results->size(); ++i) {
    const history::FaviconBitmapResult& bitmap_result =
        (*favicon_bitmap_results)[i];
    const GURL& icon_url = bitmap_result.icon_url;
    (*icon_url_sizes)[icon_url].push_back(bitmap_result.pixel_size);
  }

  RunFaviconCallbackAsync(callback, favicon_bitmap_results, icon_url_sizes);
}

// static
ChromeWebUIControllerFactory* ChromeWebUIControllerFactory::GetInstance() {
  return Singleton< ChromeWebUIControllerFactory, PossibleTestSingletonTraits<
      ChromeWebUIControllerFactory, TestChromeWebUIControllerFactory> >::get();
}

ChromeWebUIControllerFactory::ChromeWebUIControllerFactory() {
}

ChromeWebUIControllerFactory::~ChromeWebUIControllerFactory() {
}

base::RefCountedMemory* ChromeWebUIControllerFactory::GetFaviconResourceBytes(
    const GURL& page_url, ui::ScaleFactor scale_factor) const {
#if !defined(OS_ANDROID)  // Bookmarks are part of NTP on Android.
  // The bookmark manager is a chrome extension, so we have to check for it
  // before we check for extension scheme.
  if (page_url.host() == extension_misc::kBookmarkManagerId)
    return BookmarksUI::GetFaviconResourceBytes(scale_factor);

  // The extension scheme is handled in GetFaviconForURL.
  if (page_url.SchemeIs(extensions::kExtensionScheme)) {
    NOTREACHED();
    return NULL;
  }
#endif

  if (!content::GetContentClient()->HasWebUIScheme(page_url))
    return NULL;

#if defined(OS_WIN)
  if (page_url.host() == chrome::kChromeUIConflictsHost)
    return ConflictsUI::GetFaviconResourceBytes(scale_factor);
#endif

  if (page_url.host() == chrome::kChromeUICrashesHost)
    return CrashesUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host() == chrome::kChromeUIHistoryHost)
    return HistoryUI::GetFaviconResourceBytes(scale_factor);

#if !defined(OS_ANDROID)
  // Flash is not available on android.
  if (page_url.host() == chrome::kChromeUIFlashHost)
    return FlashUI::GetFaviconResourceBytes(scale_factor);

  // Android uses the native download manager.
  if (page_url.host() == chrome::kChromeUIDownloadsHost)
    return DownloadsUI::GetFaviconResourceBytes(scale_factor);

  // chrome://flags is currently unsupported on Android.
  // TODO(satish): Enable after http://crbug.com/143146 is fixed.
  if (page_url.host() == chrome::kChromeUIFlagsHost)
    return FlagsUI::GetFaviconResourceBytes(scale_factor);

  // Android doesn't use the Options pages.
  if (page_url.host() == chrome::kChromeUISettingsFrameHost)
    return options::OptionsUI::GetFaviconResourceBytes(scale_factor);

  // Android doesn't use the plugins pages.
  if (page_url.host() == chrome::kChromeUIPluginsHost)
    return PluginsUI::GetFaviconResourceBytes(scale_factor);
#endif

  return NULL;
}
