// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"

#include "base/command_line.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extensions_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/bookmarks_ui.h"
#include "chrome/browser/ui/webui/bug_report_ui.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/browser/ui/webui/downloads_ui.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/browser/ui/webui/gpu_internals_ui.h"
#include "chrome/browser/ui/webui/history2_ui.h"
#include "chrome/browser/ui/webui/history_ui.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/browser/ui/webui/net_internals_ui.h"
#include "chrome/browser/ui/webui/new_tab_ui.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/plugins_ui.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/browser/ui/webui/remoting_ui.h"
#include "chrome/browser/ui/webui/sync_internals_ui.h"
#include "chrome/browser/ui/webui/textfields_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"
#include "googleurl/src/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/choose_mobile_network_ui.h"
#include "chrome/browser/ui/webui/chromeos/enterprise_enrollment_ui.h"
#include "chrome/browser/ui/webui/chromeos/imageburner_ui.h"
#include "chrome/browser/ui/webui/chromeos/keyboard_overlay_ui.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_ui.h"
#include "chrome/browser/ui/webui/chromeos/proxy_settings_ui.h"
#include "chrome/browser/ui/webui/chromeos/register_page_ui.h"
#include "chrome/browser/ui/webui/chromeos/sim_unlock_ui.h"
#include "chrome/browser/ui/webui/chromeos/system_info_ui.h"
#include "chrome/browser/ui/webui/active_downloads_ui.h"
#include "chrome/browser/ui/webui/mediaplayer_ui.h"
#endif

#if defined(TOUCH_UI)
#include "chrome/browser/ui/webui/keyboard_ui.h"
#endif

#if defined(TOUCH_UI) && defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/login/login_ui.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/webui/conflicts_ui.h"
#endif

namespace {

// A function for creating a new WebUI. The caller owns the return value, which
// may be NULL (for example, if the URL refers to an non-existent extension).
typedef WebUI* (*WebUIFactoryFunction)(TabContents* tab_contents,
                                       const GURL& url);

// Template for defining WebUIFactoryFunction.
template<class T>
WebUI* NewWebUI(TabContents* contents, const GURL& url) {
  return new T(contents);
}

// Special case for extensions.
template<>
WebUI* NewWebUI<ExtensionWebUI>(TabContents* contents, const GURL& url) {
  // Don't use a WebUI for incognito tabs because we require extensions to run
  // within a single process.
  ExtensionService* service = contents->profile()->GetExtensionService();
  if (service &&
      service->ExtensionBindingsAllowed(url)) {
    return new ExtensionWebUI(contents, url);
  }
  return NULL;
}

// Returns a function that can be used to create the right type of WebUI for a
// tab, based on its URL. Returns NULL if the URL doesn't have WebUI associated
// with it. Even if the factory function is valid, it may yield a NULL WebUI
// when invoked for a particular tab - see NewWebUI<ExtensionWebUI>.
static WebUIFactoryFunction GetWebUIFactoryFunction(Profile* profile,
                                                    const GURL& url) {
  if (url.host() == chrome::kChromeUIDialogHost)
    return &NewWebUI<ConstrainedHtmlUI>;

  ExtensionService* service = profile ? profile->GetExtensionService() : NULL;
  if (service && service->ExtensionBindingsAllowed(url))
    return &NewWebUI<ExtensionWebUI>;

  // All platform builds of Chrome will need to have a cloud printing
  // dialog as backup.  It's just that on Chrome OS, it's the only
  // print dialog.
  if (url.host() == chrome::kCloudPrintResourcesHost)
    return &NewWebUI<ExternalHtmlDialogUI>;

  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
  if (!url.SchemeIs(chrome::kChromeDevToolsScheme) &&
      !url.SchemeIs(chrome::kChromeInternalScheme) &&
      !url.SchemeIs(chrome::kChromeUIScheme))
    return NULL;

  if (url.host() == chrome::kChromeUISyncResourcesHost ||
      url.host() == chrome::kChromeUIRemotingResourcesHost ||
      url.host() == chrome::kCloudPrintSetupHost)
    return &NewWebUI<HtmlDialogUI>;

  // Special case the new tab page. In older versions of Chrome, the new tab
  // page was hosted at chrome-internal:<blah>. This might be in people's saved
  // sessions or bookmarks, so we say any URL with that scheme triggers the new
  // tab page.
  if (url.host() == chrome::kChromeUINewTabHost ||
      url.SchemeIs(chrome::kChromeInternalScheme))
    return &NewWebUI<NewTabUI>;

  // Give about:about a generic Web UI so it can navigate to pages with Web UIs.
  if (url.spec() == chrome::kChromeUIAboutAboutURL)
    return &NewWebUI<WebUI>;

  // We must compare hosts only since some of the Web UIs append extra stuff
  // after the host name.
  if (url.host() == chrome::kChromeUIBookmarksHost)
    return &NewWebUI<BookmarksUI>;
  if (url.host() == chrome::kChromeUIBugReportHost)
    return &NewWebUI<BugReportUI>;
  if (url.host() == chrome::kChromeUICrashesHost)
    return &NewWebUI<CrashesUI>;
  if (url.host() == chrome::kChromeUIDevToolsHost)
    return &NewWebUI<DevToolsUI>;
#if defined(OS_WIN)
  if (url.host() == chrome::kChromeUIConflictsHost)
    return &NewWebUI<ConflictsUI>;
#endif
  if (url.host() == chrome::kChromeUIDownloadsHost)
    return &NewWebUI<DownloadsUI>;
  if (url.host() == chrome::kChromeUITextfieldsHost)
    return &NewWebUI<TextfieldsUI>;
  if (url.host() == chrome::kChromeUIExtensionsHost)
    return &NewWebUI<ExtensionsUI>;
  if (url.host() == chrome::kChromeUIHistoryHost)
    return &NewWebUI<HistoryUI>;
  if (url.host() == chrome::kChromeUIHistory2Host)
    return &NewWebUI<HistoryUI2>;
  if (url.host() == chrome::kChromeUIFlagsHost)
    return &NewWebUI<FlagsUI>;
#if defined(TOUCH_UI)
  if (url.host() == chrome::kChromeUIKeyboardHost)
    return &NewWebUI<KeyboardUI>;
#endif
  if (url.host() == chrome::kChromeUIGpuInternalsHost)
    return &NewWebUI<GpuInternalsUI>;
  if (url.host() == chrome::kChromeUINetInternalsHost)
    return &NewWebUI<NetInternalsUI>;
  if (url.host() == chrome::kChromeUIPluginsHost)
    return &NewWebUI<PluginsUI>;
  if (url.host() == chrome::kChromeUISyncInternalsHost)
    return &NewWebUI<SyncInternalsUI>;
#if defined(ENABLE_REMOTING)
  if (url.host() == chrome::kChromeUIRemotingHost) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableRemoting)) {
      return &NewWebUI<RemotingUI>;
    }
  }
#endif

#if defined(OS_CHROMEOS)
  if (url.host() == chrome::kChromeUIChooseMobileNetworkHost)
    return &NewWebUI<chromeos::ChooseMobileNetworkUI>;
  if (url.host() == chrome::kChromeUICollectedCookiesHost ||
      url.host() == chrome::kChromeUIHttpAuthHost) {
    return &NewWebUI<ConstrainedHtmlUI>;
  }
  if (url.host() == chrome::kChromeUIActiveDownloadsHost)
    return &NewWebUI<ActiveDownloadsUI>;
  if (url.host() == chrome::kChromeUIImageBurnerHost)
    return &NewWebUI<ImageBurnUI>;
  if (url.host() == chrome::kChromeUIKeyboardOverlayHost)
    return &NewWebUI<KeyboardOverlayUI>;
  if (url.host() == chrome::kChromeUIMediaplayerHost)
    return &NewWebUI<MediaplayerUI>;
  if (url.host() == chrome::kChromeUIMobileSetupHost)
    return &NewWebUI<MobileSetupUI>;
  if (url.host() == chrome::kChromeUIProxySettingsHost)
    return &NewWebUI<chromeos::ProxySettingsUI>;
  if (url.host() == chrome::kChromeUIRegisterPageHost)
    return &NewWebUI<RegisterPageUI>;
  if (url.host() == chrome::kChromeUISettingsHost)
    return &NewWebUI<OptionsUI>;
  if (url.host() == chrome::kChromeUISimUnlockHost)
    return &NewWebUI<chromeos::SimUnlockUI>;
  if (url.host() == chrome::kChromeUISystemInfoHost)
    return &NewWebUI<SystemInfoUI>;
  if (url.host() == chrome::kChromeUIEnterpriseEnrollmentHost)
    return &NewWebUI<chromeos::EnterpriseEnrollmentUI>;
#else
  if (url.host() == chrome::kChromeUISettingsHost)
    return &NewWebUI<OptionsUI>;
  if (url.host() == chrome::kChromeUIPrintHost) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnablePrintPreview)) {
      return &NewWebUI<PrintPreviewUI>;
    }
  }
#endif  // defined(OS_CHROMEOS)

#if defined(TOUCH_UI) && defined(OS_CHROMEOS)
  if (url.host() == chrome::kChromeUILoginHost)
    return &NewWebUI<chromeos::LoginUI>;
#endif

  if (url.spec() == chrome::kChromeUIConstrainedHTMLTestURL)
    return &NewWebUI<ConstrainedHtmlUI>;

  return NULL;
}

}  // namespace

WebUI::TypeID ChromeWebUIFactory::GetWebUIType(Profile* profile,
                                               const GURL& url) const {
  WebUIFactoryFunction function = GetWebUIFactoryFunction(profile, url);
  return function ? reinterpret_cast<WebUI::TypeID>(function) : WebUI::kNoWebUI;
}

bool ChromeWebUIFactory::UseWebUIForURL(Profile* profile,
                                        const GURL& url) const {
  return GetWebUIType(profile, url) != WebUI::kNoWebUI;
}

bool ChromeWebUIFactory::HasWebUIScheme(const GURL& url) const {
  return url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeInternalScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme) ||
         url.SchemeIs(chrome::kExtensionScheme);
}

bool ChromeWebUIFactory::IsURLAcceptableForWebUI(
    Profile* profile,
    const GURL& url) const {
  return UseWebUIForURL(profile, url) ||
      // javacsript: URLs are allowed to run in Web UI pages
      url.SchemeIs(chrome::kJavaScriptScheme) ||
      // It's possible to load about:blank in a Web UI renderer.
      // See http://crbug.com/42547
      url.spec() == chrome::kAboutBlankURL ||
      // about:crash, about:kill, about:hang, and about:shorthang are allowed.
      url.spec() == chrome::kAboutCrashURL ||
      url.spec() == chrome::kAboutKillURL ||
      url.spec() == chrome::kAboutHangURL ||
      url.spec() == chrome::kAboutShorthangURL;
}

WebUI* ChromeWebUIFactory::CreateWebUIForURL(
    TabContents* tab_contents,
    const GURL& url) const {
  WebUIFactoryFunction function = GetWebUIFactoryFunction(
      tab_contents->profile(), url);
  if (!function)
    return NULL;
  return (*function)(tab_contents, url);
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
    request->ForwardResultAsync(
        FaviconService::FaviconDataCallback::TupleType(request->handle(),
                                                       favicon));
  }
}

// static
ChromeWebUIFactory* ChromeWebUIFactory::GetInstance() {
  return Singleton<ChromeWebUIFactory>::get();
}

ChromeWebUIFactory::ChromeWebUIFactory() {
}

ChromeWebUIFactory::~ChromeWebUIFactory() {
}

RefCountedMemory* ChromeWebUIFactory::GetFaviconResourceBytes(
    const GURL& page_url) const  {
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

  if (page_url.host() == chrome::kChromeUIExtensionsHost)
    return ExtensionsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIHistoryHost)
    return HistoryUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIHistory2Host)
    return HistoryUI2::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIFlagsHost)
    return FlagsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUISettingsHost)
    return OptionsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIPluginsHost)
    return PluginsUI::GetFaviconResourceBytes();

#if defined(ENABLE_REMOTING)
  if (page_url.host() == chrome::kChromeUIRemotingHost)
    return RemotingUI::GetFaviconResourceBytes();
#endif

  return NULL;
}
