// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_factory.h"

#include "base/command_line.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/dom_ui/bookmarks_ui.h"
#include "chrome/browser/dom_ui/bug_report_ui.h"
#include "chrome/browser/dom_ui/constrained_html_ui.h"
#include "chrome/browser/dom_ui/downloads_ui.h"
#include "chrome/browser/dom_ui/devtools_ui.h"
#include "chrome/browser/dom_ui/gpu_internals_ui.h"
#include "chrome/browser/dom_ui/history_ui.h"
#include "chrome/browser/dom_ui/history2_ui.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/dom_ui/flags_ui.h"
#include "chrome/browser/dom_ui/net_internals_ui.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/dom_ui/plugins_ui.h"
#include "chrome/browser/dom_ui/print_preview_ui.h"
#include "chrome/browser/dom_ui/remoting_ui.h"
#include "chrome/browser/dom_ui/options/options_ui.h"
#include "chrome/browser/dom_ui/slideshow_ui.h"
#include "chrome/browser/dom_ui/sync_internals_ui.h"
#include "chrome/browser/dom_ui/textfields_ui.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extensions_ui.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/dom_ui/imageburner_ui.h"
#include "chrome/browser/chromeos/dom_ui/keyboard_overlay_ui.h"
#include "chrome/browser/chromeos/dom_ui/menu_ui.h"
#include "chrome/browser/chromeos/dom_ui/mobile_setup_ui.h"
#include "chrome/browser/chromeos/dom_ui/register_page_ui.h"
#include "chrome/browser/chromeos/dom_ui/system_info_ui.h"
#include "chrome/browser/chromeos/dom_ui/wrench_menu_ui.h"
#include "chrome/browser/chromeos/dom_ui/network_menu_ui.h"
#include "chrome/browser/dom_ui/filebrowse_ui.h"
#include "chrome/browser/dom_ui/mediaplayer_ui.h"
#endif

#if defined(TOUCH_UI)
#include "chrome/browser/dom_ui/keyboard_ui.h"
#include "chrome/browser/chromeos/dom_ui/login/login_ui.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/dom_ui/conflicts_ui.h"
#endif

const DOMUITypeID DOMUIFactory::kNoDOMUI = NULL;

// A function for creating a new DOMUI. The caller owns the return value, which
// may be NULL (for example, if the URL refers to an non-existent extension).
typedef DOMUI* (*DOMUIFactoryFunction)(TabContents* tab_contents,
                                       const GURL& url);

// Template for defining DOMUIFactoryFunction.
template<class T>
DOMUI* NewDOMUI(TabContents* contents, const GURL& url) {
  return new T(contents);
}

// Special case for extensions.
template<>
DOMUI* NewDOMUI<ExtensionDOMUI>(TabContents* contents, const GURL& url) {
  // Don't use a DOMUI for incognito tabs because we require extensions to run
  // within a single process.
  ExtensionService* service = contents->profile()->GetExtensionService();
  if (service &&
      service->ExtensionBindingsAllowed(url)) {
    return new ExtensionDOMUI(contents, url);
  }
  return NULL;
}

// Returns a function that can be used to create the right type of DOMUI for a
// tab, based on its URL. Returns NULL if the URL doesn't have DOMUI associated
// with it. Even if the factory function is valid, it may yield a NULL DOMUI
// when invoked for a particular tab - see NewDOMUI<ExtensionDOMUI>.
static DOMUIFactoryFunction GetDOMUIFactoryFunction(Profile* profile,
    const GURL& url) {
  // Currently, any gears: URL means an HTML dialog.
  if (url.SchemeIs(chrome::kGearsScheme))
    return &NewDOMUI<HtmlDialogUI>;

  if (url.host() == chrome::kChromeUIDialogHost)
    return &NewDOMUI<ConstrainedHtmlUI>;

  ExtensionService* service = profile ? profile->GetExtensionService() : NULL;
  if (service && service->ExtensionBindingsAllowed(url))
    return &NewDOMUI<ExtensionDOMUI>;

  // All platform builds of Chrome will need to have a cloud printing
  // dialog as backup.  It's just that on Chrome OS, it's the only
  // print dialog.
  if (url.host() == chrome::kCloudPrintResourcesHost)
    return &NewDOMUI<ExternalHtmlDialogUI>;

  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes (gears was handled above) to filter out most URLs.
  if (!url.SchemeIs(chrome::kChromeDevToolsScheme) &&
      !url.SchemeIs(chrome::kChromeInternalScheme) &&
      !url.SchemeIs(chrome::kChromeUIScheme))
    return NULL;

  if (url.host() == chrome::kChromeUISyncResourcesHost ||
      url.host() == chrome::kChromeUIRemotingResourcesHost ||
      url.host() == chrome::kCloudPrintSetupHost)
    return &NewDOMUI<HtmlDialogUI>;

  // Special case the new tab page. In older versions of Chrome, the new tab
  // page was hosted at chrome-internal:<blah>. This might be in people's saved
  // sessions or bookmarks, so we say any URL with that scheme triggers the new
  // tab page.
  if (url.host() == chrome::kChromeUINewTabHost ||
      url.SchemeIs(chrome::kChromeInternalScheme))
    return &NewDOMUI<NewTabUI>;

  // Give about:about a generic DOM UI so it can navigate to pages with DOM UIs.
  if (url.spec() == chrome::kChromeUIAboutAboutURL)
    return &NewDOMUI<DOMUI>;

  // We must compare hosts only since some of the DOM UIs append extra stuff
  // after the host name.
  if (url.host() == chrome::kChromeUIBookmarksHost)
    return &NewDOMUI<BookmarksUI>;
  if (url.host() == chrome::kChromeUIBugReportHost)
    return &NewDOMUI<BugReportUI>;
  if (url.host() == chrome::kChromeUIDevToolsHost)
    return &NewDOMUI<DevToolsUI>;
#if defined(OS_WIN)
  if (url.host() == chrome::kChromeUIConflictsHost)
    return &NewDOMUI<ConflictsUI>;
#endif
  if (url.host() == chrome::kChromeUIDownloadsHost)
    return &NewDOMUI<DownloadsUI>;
  if (url.host() == chrome::kChromeUITextfieldsHost)
    return &NewDOMUI<TextfieldsUI>;
  if (url.host() == chrome::kChromeUIExtensionsHost)
    return &NewDOMUI<ExtensionsUI>;
  if (url.host() == chrome::kChromeUIHistoryHost)
    return &NewDOMUI<HistoryUI>;
  if (url.host() == chrome::kChromeUIHistory2Host)
    return &NewDOMUI<HistoryUI2>;
  if (url.host() == chrome::kChromeUIFlagsHost)
    return &NewDOMUI<FlagsUI>;
#if defined(TOUCH_UI)
  if (url.host() == chrome::kChromeUIKeyboardHost)
    return &NewDOMUI<KeyboardUI>;
  if (url.host() == chrome::kChromeUILoginHost)
    return &NewDOMUI<chromeos::LoginUI>;
#endif
  if (url.host() == chrome::kChromeUIGpuInternalsHost)
    return &NewDOMUI<GpuInternalsUI>;
  if (url.host() == chrome::kChromeUINetInternalsHost)
    return &NewDOMUI<NetInternalsUI>;
  if (url.host() == chrome::kChromeUIPluginsHost)
    return &NewDOMUI<PluginsUI>;
  if (url.host() == chrome::kChromeUISyncInternalsHost)
    return &NewDOMUI<SyncInternalsUI>;
#if defined(ENABLE_REMOTING)
  if (url.host() == chrome::kChromeUIRemotingHost) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableRemoting)) {
      return &NewDOMUI<RemotingUI>;
    }
  }
#endif

#if defined(OS_CHROMEOS)
  if (url.host() == chrome::kChromeUIFileBrowseHost)
    return &NewDOMUI<FileBrowseUI>;
  if (url.host() == chrome::kChromeUIImageBurnerHost)
    return &NewDOMUI<ImageBurnUI>;
  if (url.host() == chrome::kChromeUIKeyboardOverlayHost)
    return &NewDOMUI<KeyboardOverlayUI>;
  if (url.host() == chrome::kChromeUIMediaplayerHost)
    return &NewDOMUI<MediaplayerUI>;
  if (url.host() == chrome::kChromeUIMobileSetupHost)
    return &NewDOMUI<MobileSetupUI>;
  if (url.host() == chrome::kChromeUIRegisterPageHost)
    return &NewDOMUI<RegisterPageUI>;
  if (url.host() == chrome::kChromeUISettingsHost)
    return &NewDOMUI<OptionsUI>;
  if (url.host() == chrome::kChromeUISlideshowHost)
    return &NewDOMUI<SlideshowUI>;
  if (url.host() == chrome::kChromeUISystemInfoHost)
    return &NewDOMUI<SystemInfoUI>;
  if (url.host() == chrome::kChromeUIMenu)
    return &NewDOMUI<chromeos::MenuUI>;
  if (url.host() == chrome::kChromeUIWrenchMenu)
    return &NewDOMUI<chromeos::WrenchMenuUI>;
  if (url.host() == chrome::kChromeUINetworkMenu)
    return &NewDOMUI<chromeos::NetworkMenuUI>;
#else
  if (url.host() == chrome::kChromeUISettingsHost) {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableTabbedOptions)) {
      return &NewDOMUI<OptionsUI>;
    }
  }
  if (url.host() == chrome::kChromeUIPrintHost) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnablePrintPreview)) {
      return &NewDOMUI<PrintPreviewUI>;
    }
  }
#endif  // defined(OS_CHROMEOS)

  if (url.spec() == chrome::kChromeUIConstrainedHTMLTestURL)
    return &NewDOMUI<ConstrainedHtmlUI>;

  return NULL;
}

// static
DOMUITypeID DOMUIFactory::GetDOMUIType(Profile* profile, const GURL& url) {
  DOMUIFactoryFunction function = GetDOMUIFactoryFunction(profile, url);
  return function ? reinterpret_cast<DOMUITypeID>(function) : kNoDOMUI;
}

// static
bool DOMUIFactory::HasDOMUIScheme(const GURL& url) {
  return url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeInternalScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme) ||
         url.SchemeIs(chrome::kExtensionScheme);
}

// static
bool DOMUIFactory::UseDOMUIForURL(Profile* profile, const GURL& url) {
  return GetDOMUIFactoryFunction(profile, url) != NULL;
}

// static
bool DOMUIFactory::IsURLAcceptableForDOMUI(Profile* profile, const GURL& url) {
  return UseDOMUIForURL(profile, url) ||
      // javacsript: URLs are allowed to run in DOM UI pages
      url.SchemeIs(chrome::kJavaScriptScheme) ||
      // It's possible to load about:blank in a DOM UI renderer.
      // See http://crbug.com/42547
      url.spec() == chrome::kAboutBlankURL ||
      // about:crash, about:kill, about:hang, and about:shorthang are allowed.
      url.spec() == chrome::kAboutCrashURL ||
      url.spec() == chrome::kAboutKillURL ||
      url.spec() == chrome::kAboutHangURL ||
      url.spec() == chrome::kAboutShorthangURL;
}

// static
DOMUI* DOMUIFactory::CreateDOMUIForURL(TabContents* tab_contents,
                                       const GURL& url) {
  DOMUIFactoryFunction function = GetDOMUIFactoryFunction(
      tab_contents->profile(), url);
  if (!function)
    return NULL;
  return (*function)(tab_contents, url);
}

// static
void DOMUIFactory::GetFaviconForURL(Profile* profile,
                                    FaviconService::GetFaviconRequest* request,
                                    const GURL& page_url) {
  // All extensions but the bookmark manager get their favicon from the icons
  // part of the manifest.
  if (page_url.SchemeIs(chrome::kExtensionScheme) &&
      page_url.host() != extension_misc::kBookmarkManagerId) {
    ExtensionDOMUI::GetFaviconForURL(profile, request, page_url);
  } else {
    scoped_refptr<RefCountedMemory> icon_data(
        DOMUIFactory::GetFaviconResourceBytes(profile, page_url));
    bool know_icon = icon_data.get() != NULL && icon_data->size() > 0;
    request->ForwardResultAsync(
        FaviconService::FaviconDataCallback::TupleType(request->handle(),
            know_icon, icon_data, false, GURL()));
  }
}

// static
RefCountedMemory* DOMUIFactory::GetFaviconResourceBytes(Profile* profile,
                                                        const GURL& page_url) {
  // The bookmark manager is a chrome extension, so we have to check for it
  // before we check for extension scheme.
  if (page_url.host() == extension_misc::kBookmarkManagerId)
    return BookmarksUI::GetFaviconResourceBytes();

  // The extension scheme is handled in GetFaviconForURL.
  if (page_url.SchemeIs(chrome::kExtensionScheme)) {
    NOTREACHED();
    return NULL;
  }

  if (!HasDOMUIScheme(page_url))
    return NULL;

#if defined(OS_WIN)
  if (page_url.host() == chrome::kChromeUIConflictsHost)
    return ConflictsUI::GetFaviconResourceBytes();
#endif

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
