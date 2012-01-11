// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"

#include "base/command_line.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/about_page/about_page_ui.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/bookmarks_ui.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/browser/ui/webui/downloads_ui.h"
#include "chrome/browser/ui/webui/edit_search_engine_dialog_ui_webui.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "chrome/browser/ui/webui/feedback_ui.h"
#include "chrome/browser/ui/webui/task_manager_ui.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/browser/ui/webui/flash_ui.h"
#include "chrome/browser/ui/webui/gpu_internals_ui.h"
#include "chrome/browser/ui/webui/history_ui.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/browser/ui/webui/hung_renderer_dialog_ui.h"
#include "chrome/browser/ui/webui/media/media_internals_ui.h"
#include "chrome/browser/ui/webui/net_internals_ui.h"
#include "chrome/browser/ui/webui/network_action_predictor/network_action_predictor_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "chrome/browser/ui/webui/plugins_ui.h"
#include "chrome/browser/ui/webui/policy_ui.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/browser/ui/webui/profiler_ui.h"
#include "chrome/browser/ui/webui/quota_internals_ui.h"
#include "chrome/browser/ui/webui/sessions_ui.h"
#include "chrome/browser/ui/webui/sync_internals_ui.h"
#include "chrome/browser/ui/webui/test_chrome_web_ui_factory.h"
#include "chrome/browser/ui/webui/tracing_ui.h"
#include "chrome/browser/ui/webui/uber/uber_ui.h"
#include "chrome/browser/ui/webui/workers_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/browser/webui/web_ui.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/choose_mobile_network_ui.h"
#include "chrome/browser/ui/webui/chromeos/imageburner/imageburner_ui.h"
#include "chrome/browser/ui/webui/chromeos/keyboard_overlay_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_ui.h"
#include "chrome/browser/ui/webui/chromeos/proxy_settings_ui.h"
#include "chrome/browser/ui/webui/chromeos/register_page_ui.h"
#include "chrome/browser/ui/webui/chromeos/sim_unlock_ui.h"
#include "chrome/browser/ui/webui/chromeos/system_info_ui.h"
#include "chrome/browser/ui/webui/active_downloads_ui.h"
#else
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#endif

#if defined(USE_VIRTUAL_KEYBOARD)
#include "chrome/browser/ui/webui/keyboard_ui.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/webui/conflicts_ui.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "chrome/browser/ui/webui/certificate_viewer_ui.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/webui/aura/app_list_ui.h"
#else
#include "chrome/browser/ui/webui/input_window_dialog_ui.h"
#endif

using content::WebContents;

namespace {

// A function for creating a new WebUI. The caller owns the return value, which
// may be NULL (for example, if the URL refers to an non-existent extension).
typedef WebUI* (*WebUIFactoryFunction)(WebContents* web_contents,
                                       const GURL& url);

// Template for defining WebUIFactoryFunction.
template<class T>
WebUI* NewWebUI(WebContents* contents, const GURL& url) {
  return new T(contents);
}

// Special case for extensions.
template<>
WebUI* NewWebUI<ExtensionWebUI>(WebContents* contents, const GURL& url) {
  return new ExtensionWebUI(contents, url);
}

// Special case for older about: handlers.
template<>
WebUI* NewWebUI<AboutUI>(WebContents* contents, const GURL& url) {
  return new AboutUI(contents, url.host());
}

// Only create ExtensionWebUI for URLs that are allowed extension bindings,
// hosted by actual tabs. If tab_contents has no wrapper, it likely refers
// to another container type, like an extension background page. If there is
// no tab_contents (it's not accessible when calling GetWebUIType and related
// functions) then we conservatively assume that we need a WebUI.
bool NeedsExtensionWebUI(WebContents* web_contents,
                         Profile* profile,
                         const GURL& url) {
  ExtensionService* service = profile ? profile->GetExtensionService() : NULL;
  return service && service->ExtensionBindingsAllowed(url) &&
      (!web_contents ||
        TabContentsWrapper::GetCurrentWrapperForContents(web_contents));
}

// Returns a function that can be used to create the right type of WebUI for a
// tab, based on its URL. Returns NULL if the URL doesn't have WebUI associated
// with it.
WebUIFactoryFunction GetWebUIFactoryFunction(WebContents* web_contents,
                                             Profile* profile,
                                             const GURL& url) {
  if (NeedsExtensionWebUI(web_contents, profile, url))
    return &NewWebUI<ExtensionWebUI>;

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
  if (url.host() == chrome::kChromeUIBookmarksHost)
    return &NewWebUI<BookmarksUI>;
  // All platform builds of Chrome will need to have a cloud printing
  // dialog as backup.  It's just that on Chrome OS, it's the only
  // print dialog.
  if (url.host() == chrome::kChromeUICloudPrintResourcesHost)
    return &NewWebUI<ExternalHtmlDialogUI>;
  if (url.host() == chrome::kChromeUICloudPrintSetupHost)
    return &NewWebUI<HtmlDialogUI>;
  if (url.spec() == chrome::kChromeUIConstrainedHTMLTestURL)
    return &NewWebUI<ConstrainedHtmlUI>;
  if (url.host() == chrome::kChromeUICrashesHost)
    return &NewWebUI<CrashesUI>;
  if (url.host() == chrome::kChromeUIDevToolsHost)
    return &NewWebUI<DevToolsUI>;
  if (url.host() == chrome::kChromeUIDialogHost)
    return &NewWebUI<ConstrainedHtmlUI>;
  if (url.host() == chrome::kChromeUIDownloadsHost)
    return &NewWebUI<DownloadsUI>;
  if (url.host() == chrome::kChromeUIEditSearchEngineDialogHost)
    return &NewWebUI<EditSearchEngineDialogUI>;
  if (url.host() == chrome::kChromeUIExtensionsFrameHost)
    return &NewWebUI<ExtensionsUI>;
  if (url.host() == chrome::kChromeUIFeedbackHost)
    return &NewWebUI<FeedbackUI>;
  if (url.host() == chrome::kChromeUIFlagsHost)
    return &NewWebUI<FlagsUI>;
  if (url.host() == chrome::kChromeUIFlashHost)
    return &NewWebUI<FlashUI>;
  if (url.host() == chrome::kChromeUIGpuInternalsHost)
    return &NewWebUI<GpuInternalsUI>;
  if (url.host() == chrome::kChromeUIHistoryHost)
    return &NewWebUI<HistoryUI>;
  if (url.host() == chrome::kChromeUIHungRendererDialogHost)
    return &NewWebUI<HungRendererDialogUI>;
  if (url.host() == chrome::kChromeUIMediaInternalsHost)
    return &NewWebUI<MediaInternalsUI>;
  if (url.host() == chrome::kChromeUINetInternalsHost)
    return &NewWebUI<NetInternalsUI>;
  if (url.host() == chrome::kChromeUINetworkActionPredictorHost)
    return &NewWebUI<NetworkActionPredictorUI>;
  if (url.host() == chrome::kChromeUIPluginsHost)
    return &NewWebUI<PluginsUI>;
  if (url.host() == chrome::kChromeUIProfilerHost)
    return &NewWebUI<ProfilerUI>;
  if (url.host() == chrome::kChromeUIQuotaInternalsHost)
    return &NewWebUI<QuotaInternalsUI>;
  if (url.host() == chrome::kChromeUISSLClientCertificateSelectorHost)
    return &NewWebUI<ConstrainedHtmlUI>;
  if (url.host() == chrome::kChromeUISettingsFrameHost)
    return &NewWebUI<options2::OptionsUI>;
  if (url.host() == chrome::kChromeUISessionsHost)
    return &NewWebUI<SessionsUI>;
  if (url.host() == chrome::kChromeUISettingsHost)
    return &NewWebUI<OptionsUI>;
  if (url.host() == chrome::kChromeUISyncInternalsHost)
    return &NewWebUI<SyncInternalsUI>;
  if (url.host() == chrome::kChromeUISyncResourcesHost)
    return &NewWebUI<HtmlDialogUI>;
  if (url.host() == chrome::kChromeUITaskManagerHost)
    return &NewWebUI<TaskManagerUI>;
  if (url.host() == chrome::kChromeUITracingHost)
    return &NewWebUI<TracingUI>;
  if (url.host() == chrome::kChromeUIUberHost)
    return &NewWebUI<UberUI>;
  if (url.host() == chrome::kChromeUIWorkersHost)
    return &NewWebUI<WorkersUI>;

  /****************************************************************************
   * OS Specific #defines
   ***************************************************************************/
#if defined(OS_WIN)
  if (url.host() == chrome::kChromeUIConflictsHost)
    return &NewWebUI<ConflictsUI>;
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  if (url.host() == chrome::kChromeUICertificateViewerHost)
    return &NewWebUI<CertificateViewerUI>;
#endif
#if defined(OS_CHROMEOS)
  if (url.host() == chrome::kChromeUIActiveDownloadsHost)
    return &NewWebUI<ActiveDownloadsUI>;
  if (url.host() == chrome::kChromeUIChooseMobileNetworkHost)
    return &NewWebUI<chromeos::ChooseMobileNetworkUI>;
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
    return &NewWebUI<SystemInfoUI>;
  if (url.host() == chrome::kChromeUIAboutPageFrameHost)
    return &NewWebUI<AboutPageUI>;
#endif  // defined(OS_CHROMEOS)

  /****************************************************************************
   * Other #defines and special logics.
   ***************************************************************************/
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (url.host() == chrome::kChromeUIPolicyHost)
    return &NewWebUI<PolicyUI>;
#endif
#if defined(USE_VIRTUAL_KEYBOARD)
  if (url.host() == chrome::kChromeUIKeyboardHost)
    return &NewWebUI<KeyboardUI>;
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
  if (url.host() == chrome::kChromeUICollectedCookiesHost ||
      url.host() == chrome::kChromeUIHttpAuthHost ||
      url.host() == chrome::kChromeUITabModalConfirmDialogHost) {
    return &NewWebUI<ConstrainedHtmlUI>;
  }
#endif

#if defined(USE_AURA)
  if (url.host() == chrome::kChromeUIAppListHost)
    return &NewWebUI<AppListUI>;
#else
  if (url.host() == chrome::kChromeUIInputWindowDialogHost)
    return &NewWebUI<InputWindowDialogUI>;
#endif

  if (url.host() == chrome::kChromeUIPrintHost &&
      switches::IsPrintPreviewEnabled()) {
    return &NewWebUI<PrintPreviewUI>;
  }

#if !defined(OS_CHROMEOS)
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
      url.host() == chrome::kChromeUIHistogramsHost ||
      url.host() == chrome::kChromeUIMemoryHost ||
      url.host() == chrome::kChromeUIMemoryRedirectHost ||
      url.host() == chrome::kChromeUIStatsHost ||
      url.host() == chrome::kChromeUITaskManagerHost ||
      url.host() == chrome::kChromeUITermsHost ||
      url.host() == chrome::kChromeUIVersionHost
#if defined(USE_TCMALLOC)
      || url.host() == chrome::kChromeUITCMallocHost
#endif
#if defined(OS_LINUX) || defined(OS_OPENBSD)
      || url.host() == chrome::kChromeUILinuxProxyConfigHost
      || url.host() == chrome::kChromeUISandboxHost
#endif
#if defined(OS_CHROMEOS)
      || url.host() == chrome::kChromeUICryptohomeHost
      || url.host() == chrome::kChromeUIDiscardsHost
      || url.host() == chrome::kChromeUINetworkHost
      || url.host() == chrome::kChromeUIOSCreditsHost
#endif
      ) {
    return &NewWebUI<AboutUI>;
  }

  DLOG(WARNING) << "Unknown WebUI:" << url;
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

}  // namespace

WebUI::TypeID ChromeWebUIFactory::GetWebUIType(
      content::BrowserContext* browser_context, const GURL& url) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebUIFactoryFunction function = GetWebUIFactoryFunction(NULL, profile, url);
  return function ? reinterpret_cast<WebUI::TypeID>(function) : WebUI::kNoWebUI;
}

bool ChromeWebUIFactory::UseWebUIForURL(
    content::BrowserContext* browser_context, const GURL& url) const {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

bool ChromeWebUIFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context, const GURL& url) const {
  // Extensions are rendered via WebUI in tabs, but don't actually need WebUI
  // bindings (see the ExtensionWebUI constructor).
  return !NeedsExtensionWebUI(NULL,
                              Profile::FromBrowserContext(browser_context),
                              url) &&
      UseWebUIForURL(browser_context, url);
}

bool ChromeWebUIFactory::HasWebUIScheme(const GURL& url) const {
  return url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeInternalScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme);
}

bool ChromeWebUIFactory::IsURLAcceptableForWebUI(
    content::BrowserContext* browser_context,
    const GURL& url) const {
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
      url == GURL(chrome::kChromeUIShorthangURL);
}

WebUI* ChromeWebUIFactory::CreateWebUIForURL(
    content::WebContents* web_contents,
    const GURL& url) const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  WebUIFactoryFunction function = GetWebUIFactoryFunction(web_contents,
                                                          profile, url);
  if (!function)
    return NULL;
  return (*function)(web_contents, url);
}

void ChromeWebUIFactory::GetFaviconForURL(
    Profile* profile,
    FaviconService::GetFaviconRequest* request,
    const GURL& page_url) const {
  // All extensions but the bookmark manager get their favicon from the icons
  // part of the manifest.
  if (page_url.SchemeIs(chrome::kExtensionScheme) &&
      page_url.host() != extension_misc::kBookmarkManagerId) {
    ExtensionWebUI::GetFaviconForURL(profile, request, page_url);
  } else {
    history::FaviconData favicon;
    favicon.image_data = scoped_refptr<RefCountedMemory>(
        GetFaviconResourceBytes(page_url));
    favicon.known_icon = favicon.image_data.get() != NULL &&
                             favicon.image_data->size() > 0;
    favicon.icon_type = history::FAVICON;
    request->ForwardResultAsync(request->handle(), favicon);
  }
}

// static
ChromeWebUIFactory* ChromeWebUIFactory::GetInstance() {
  return Singleton< ChromeWebUIFactory, PossibleTestSingletonTraits<
      ChromeWebUIFactory, TestChromeWebUIFactory> >::get();
}

ChromeWebUIFactory::ChromeWebUIFactory() {
}

ChromeWebUIFactory::~ChromeWebUIFactory() {
}

RefCountedMemory* ChromeWebUIFactory::GetFaviconResourceBytes(
    const GURL& page_url) const {
  // The bookmark manager is a chrome extension, so we have to check for it
  // before we check for extension scheme.
  if (page_url.host() == extension_misc::kBookmarkManagerId)
    return BookmarksUI::GetFaviconResourceBytes();

  // The extension scheme is handled in GetFaviconForURL.
  if (page_url.SchemeIs(chrome::kExtensionScheme)) {
    NOTREACHED();
    return NULL;
  }

  if (!HasWebUIScheme(page_url))
    return NULL;

#if defined(OS_WIN)
  if (page_url.host() == chrome::kChromeUIConflictsHost)
    return ConflictsUI::GetFaviconResourceBytes();
#endif

  if (page_url.host() == chrome::kChromeUICrashesHost)
    return CrashesUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIDownloadsHost)
    return DownloadsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIHistoryHost)
    return HistoryUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIFlagsHost)
    return FlagsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUISessionsHost)
    return SessionsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIFlashHost)
    return FlashUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUISettingsHost)
    return OptionsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUISettingsFrameHost)
    return options2::OptionsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIPluginsHost)
    return PluginsUI::GetFaviconResourceBytes();

  return NULL;
}
