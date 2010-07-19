// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_factory.h"

#include "base/command_line.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/bookmarks_ui.h"
#include "chrome/browser/dom_ui/downloads_ui.h"
#include "chrome/browser/dom_ui/devtools_ui.h"
#include "chrome/browser/dom_ui/history_ui.h"
#include "chrome/browser/dom_ui/history2_ui.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/dom_ui/net_internals_ui.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/dom_ui/options_ui.h"
#include "chrome/browser/dom_ui/remoting_ui.h"
#include "chrome/browser/dom_ui/plugins_ui.h"
#include "chrome/browser/dom_ui/slideshow_ui.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extensions_ui.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/dom_ui/filebrowse_ui.h"
#include "chrome/browser/dom_ui/mediaplayer_ui.h"
#include "chrome/browser/dom_ui/register_page_ui.h"
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
  // Don't use a DOMUI for non-existent extensions or for incognito tabs. The
  // latter restriction is because we require extensions to run within a single
  // process.
  ExtensionsService* service = contents->profile()->GetExtensionsService();
  bool valid_extension =
      (service && service->GetExtensionById(url.host(), false));
  if (valid_extension && !contents->profile()->IsOffTheRecord())
    return new ExtensionDOMUI(contents);
  return NULL;
}

// Returns a function that can be used to create the right type of DOMUI for a
// tab, based on its URL. Returns NULL if the URL doesn't have DOMUI associated
// with it. Even if the factory function is valid, it may yield a NULL DOMUI
// when invoked for a particular tab - see NewDOMUI<ExtensionDOMUI>.
static DOMUIFactoryFunction GetDOMUIFactoryFunction(const GURL& url) {
  // Currently, any gears: URL means an HTML dialog.
  if (url.SchemeIs(chrome::kGearsScheme))
    return &NewDOMUI<HtmlDialogUI>;

  if (url.SchemeIs(chrome::kExtensionScheme))
    return &NewDOMUI<ExtensionDOMUI>;

  // All platform builds of Chrome will need to have a cloud printing
  // dialog as backup.  It's just that on Chrome OS, it's the only
  // print dialog.
  if (url.host() == chrome::kCloudPrintResourcesHost)
    return &NewDOMUI<ExternalHtmlDialogUI>;

  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes (gears was handled above) to filter out most URLs.
  if (!url.SchemeIs(chrome::kChromeInternalScheme) &&
      !url.SchemeIs(chrome::kChromeUIScheme))
    return NULL;

  if (url.host() == chrome::kChromeUISyncResourcesHost)
    return &NewDOMUI<HtmlDialogUI>;

  // Special case the new tab page. In older versions of Chrome, the new tab
  // page was hosted at chrome-internal:<blah>. This might be in people's saved
  // sessions or bookmarks, so we say any URL with that scheme triggers the new
  // tab page.
  if (url.host() == chrome::kChromeUINewTabHost ||
      url.SchemeIs(chrome::kChromeInternalScheme))
    return &NewDOMUI<NewTabUI>;

  // We must compare hosts only since some of the DOM UIs append extra stuff
  // after the host name.
  if (url.host() == chrome::kChromeUIBookmarksHost)
    return &NewDOMUI<BookmarksUI>;
  if (url.host() == chrome::kChromeUIDevToolsHost)
    return &NewDOMUI<DevToolsUI>;
  if (url.host() == chrome::kChromeUIDownloadsHost)
    return &NewDOMUI<DownloadsUI>;
  if (url.host() == chrome::kChromeUIExtensionsHost)
    return &NewDOMUI<ExtensionsUI>;
  if (url.host() == chrome::kChromeUIHistoryHost)
    return &NewDOMUI<HistoryUI>;
  if (url.host() == chrome::kChromeUIHistory2Host)
    return &NewDOMUI<HistoryUI2>;
  if (url.host() == chrome::kChromeUINetInternalsHost)
    return &NewDOMUI<NetInternalsUI>;
  if (url.host() == chrome::kChromeUIPluginsHost)
    return &NewDOMUI<PluginsUI>;
#if defined(ENABLE_REMOTING)
  if (url.host() == chrome::kChromeUIRemotingHost) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableChromoting)) {
      return &NewDOMUI<RemotingUI>;
    }
  }
#endif

#if defined(OS_CHROMEOS)
  if (url.host() == chrome::kChromeUIFileBrowseHost)
    return &NewDOMUI<FileBrowseUI>;
  if (url.host() == chrome::kChromeUIMediaplayerHost)
    return &NewDOMUI<MediaplayerUI>;
  if (url.host() == chrome::kChromeUIRegisterPageHost)
    return &NewDOMUI<RegisterPageUI>;
  if (url.host() == chrome::kChromeUISlideshowHost)
    return &NewDOMUI<SlideshowUI>;
  if (url.host() == chrome::kChromeUIOptionsHost)
    return &NewDOMUI<OptionsUI>;
#else
  if (url.host() == chrome::kChromeUIOptionsHost) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableTabbedOptions)) {
      return &NewDOMUI<OptionsUI>;
    }
  }
#endif

  return NULL;
}

// static
DOMUITypeID DOMUIFactory::GetDOMUIType(const GURL& url) {
  DOMUIFactoryFunction function = GetDOMUIFactoryFunction(url);
  return function ? reinterpret_cast<DOMUITypeID>(function) : kNoDOMUI;
}

// static
bool DOMUIFactory::HasDOMUIScheme(const GURL& url) {
  return url.SchemeIs(chrome::kChromeInternalScheme) ||
         url.SchemeIs(chrome::kChromeUIScheme) ||
         url.SchemeIs(chrome::kExtensionScheme);
}

// static
bool DOMUIFactory::UseDOMUIForURL(const GURL& url) {
  return GetDOMUIFactoryFunction(url) != NULL;
}

// static
DOMUI* DOMUIFactory::CreateDOMUIForURL(TabContents* tab_contents,
                                       const GURL& url) {
  DOMUIFactoryFunction function = GetDOMUIFactoryFunction(url);
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
    scoped_refptr<RefCountedMemory> icon_data =
        DOMUIFactory::GetFaviconResourceBytes(profile, page_url);
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

  if (page_url.host() == chrome::kChromeUIDownloadsHost)
    return DownloadsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIExtensionsHost)
    return ExtensionsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIHistoryHost)
    return HistoryUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIHistory2Host)
    return HistoryUI2::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIOptionsHost)
    return OptionsUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIPluginsHost)
    return PluginsUI::GetFaviconResourceBytes();

#if defined(ENABLE_REMOTING)
  if (page_url.host() == chrome::kChromeUIRemotingHost)
    return RemotingUI::GetFaviconResourceBytes();
#endif

  return NULL;
}
