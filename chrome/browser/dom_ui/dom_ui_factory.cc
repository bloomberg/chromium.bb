// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_factory.h"

#include "chrome/browser/dom_ui/downloads_ui.h"
#include "chrome/browser/dom_ui/devtools_ui.h"
#include "chrome/browser/dom_ui/history_ui.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/dom_ui/print_ui.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extensions_ui.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

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
  // Don't use a DOMUI for non-existent extensions.
  ExtensionsService* service = contents->profile()->GetExtensionsService();
  bool valid_extension = (service && service->GetExtensionById(url.host()));
  if (valid_extension)
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

// TODO(mhm) Make sure this ifdef is removed once print is complete.
#if !defined(GOOGLE_CHROME_BUILD)
  if (url.SchemeIs(chrome::kPrintScheme))
    return &NewDOMUI<PrintUI>;
#endif

  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes (gears was handled above) to filter out most URLs.
  if (!url.SchemeIs(chrome::kChromeInternalScheme) &&
      !url.SchemeIs(chrome::kChromeUIScheme))
    return NULL;

  if (url.host() == chrome::kSyncResourcesHost)
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
  if (url.host() == chrome::kChromeUIHistoryHost)
    return &NewDOMUI<HistoryUI>;
  if (url.host() == chrome::kChromeUIDownloadsHost)
    return &NewDOMUI<DownloadsUI>;
  if (url.host() == chrome::kChromeUIExtensionsHost)
    return &NewDOMUI<ExtensionsUI>;
  if (url.host() == chrome::kChromeUIDevToolsHost)
    return &NewDOMUI<DevToolsUI>;

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
         url.SchemeIs(chrome::kExtensionScheme) ||
         url.SchemeIs(chrome::kPrintScheme);
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
RefCountedMemory* DOMUIFactory::GetFaviconResourceBytes(
    const GURL& page_url) {
  if (!HasDOMUIScheme(page_url))
    return NULL;

  if (page_url.host() == chrome::kChromeUIHistoryHost)
    return HistoryUI::GetFaviconResourceBytes();

  if (page_url.host() == chrome::kChromeUIDownloadsHost)
    return DownloadsUI::GetFaviconResourceBytes();

  return NULL;
}
