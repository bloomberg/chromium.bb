// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "url/gurl.h"

using content::NavigationEntry;
using content::WebContents;

namespace extensions {

namespace keys = tabs_constants;

namespace {

WindowController* GetAppWindowController(const WebContents* contents) {
  AppWindowRegistry* registry =
      AppWindowRegistry::Get(contents->GetBrowserContext());
  if (!registry)
    return NULL;
  AppWindow* app_window =
      registry->GetAppWindowForRenderViewHost(contents->GetRenderViewHost());
  if (!app_window)
    return NULL;
  return WindowControllerList::GetInstance()->FindWindowById(
      app_window->session_id().id());
}

}  // namespace

ExtensionTabUtil::OpenTabParams::OpenTabParams()
    : create_browser_if_needed(false) {
}

ExtensionTabUtil::OpenTabParams::~OpenTabParams() {
}

// Opens a new tab for a given extension. Returns NULL and sets |error| if an
// error occurs.
base::DictionaryValue* ExtensionTabUtil::OpenTab(
    ChromeUIThreadExtensionFunction* function,
    const OpenTabParams& params,
    std::string* error) {
  NOTIMPLEMENTED();
  return NULL;
}

Browser* ExtensionTabUtil::GetBrowserFromWindowID(
    ChromeUIThreadExtensionFunction* function,
    int window_id,
    std::string* error) {
  NOTREACHED();
  return NULL;
}

Browser* ExtensionTabUtil::GetBrowserFromWindowID(
    const ChromeExtensionFunctionDetails& details,
    int window_id,
    std::string* error) {
  NOTREACHED();
  return NULL;
}

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  NOTREACHED();
  return -1;
}

int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  NOTREACHED();
  return -1;
}

int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  return SessionTabHelper::IdForTab(web_contents);
}

std::string ExtensionTabUtil::GetTabStatusText(bool is_loading) {
  return is_loading ? keys::kStatusValueLoading : keys::kStatusValueComplete;
}

int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  return SessionTabHelper::IdForWindowContainingTab(web_contents);
}

base::DictionaryValue* ExtensionTabUtil::CreateTabValue(
    WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index,
    const Extension* extension) {
  NOTREACHED();
  return NULL;
}

base::ListValue* ExtensionTabUtil::CreateTabList(
    const Browser* browser,
    const Extension* extension) {
  return new base::ListValue();
}

base::DictionaryValue* ExtensionTabUtil::CreateTabValue(
    WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index) {
  // There's no TabStrip in Athena.
  DCHECK(!tab_strip);

  // If we have a matching AppWindow with a controller, get the tab value
  // from its controller instead.
  WindowController* controller = GetAppWindowController(contents);
  if (controller)
    return controller->CreateTabValue(NULL, tab_index);

  base::DictionaryValue* result = new base::DictionaryValue();
  bool is_loading = contents->IsLoading();
  result->SetInteger(keys::kIdKey, GetTabId(contents));
  result->SetInteger(keys::kIndexKey, tab_index);
  result->SetInteger(keys::kWindowIdKey, GetWindowIdOfTab(contents));
  result->SetString(keys::kStatusKey, GetTabStatusText(is_loading));
  result->SetBoolean(keys::kActiveKey, false);
  result->SetBoolean(keys::kSelectedKey, false);
  result->SetBoolean(keys::kHighlightedKey, false);
  result->SetBoolean(keys::kPinnedKey, false);
  result->SetBoolean(keys::kIncognitoKey,
                     contents->GetBrowserContext()->IsOffTheRecord());
  result->SetInteger(keys::kWidthKey,
                     contents->GetContainerBounds().size().width());
  result->SetInteger(keys::kHeightKey,
                     contents->GetContainerBounds().size().height());

  // Privacy-sensitive fields: these should be stripped off by
  // ScrubTabValueForExtension if the extension should not see them.
  result->SetString(keys::kUrlKey, contents->GetURL().spec());
  result->SetString(keys::kTitleKey, contents->GetTitle());
  if (!is_loading) {
    NavigationEntry* entry = contents->GetController().GetVisibleEntry();
    if (entry && entry->GetFavicon().valid)
      result->SetString(keys::kFaviconUrlKey, entry->GetFavicon().url.spec());
  }

  return result;
}

void ExtensionTabUtil::ScrubTabValueForExtension(
    WebContents* contents,
    const Extension* extension,
    base::DictionaryValue* tab_info) {
  // TODO(oshima): Move this to common impl.
}

void ExtensionTabUtil::ScrubTabForExtension(const Extension* extension,
                                            api::tabs::Tab* tab) {

  // TODO(oshima): Move this to common impl.
}

bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  NOTIMPLEMENTED();

  return false;
}

bool ExtensionTabUtil::GetDefaultTab(Browser* browser,
                                     WebContents** contents,
                                     int* tab_id) {
  NOTIMPLEMENTED();
  return false;
}

bool ExtensionTabUtil::GetTabById(int tab_id,
                                  content::BrowserContext* browser_context,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  WebContents** contents,
                                  int* tab_index) {
  NOTIMPLEMENTED();
  return false;
}

GURL ExtensionTabUtil::ResolvePossiblyRelativeURL(const std::string& url_string,
                                                  const Extension* extension) {
  // TODO(oshima): Move this to common impl.
  return GURL(url_string);
}

bool ExtensionTabUtil::IsCrashURL(const GURL& url) {
  // TODO(oshima): Move this to common impl.
  return false;
}

void ExtensionTabUtil::CreateTab(WebContents* web_contents,
                                 const std::string& extension_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {
  NOTIMPLEMENTED();
}

// static
void ExtensionTabUtil::ForEachTab(
    const base::Callback<void(WebContents*)>& callback) {
  // TODO(oshima): Move this to common impl.
}

// static
WindowController* ExtensionTabUtil::GetWindowControllerOfTab(
    const WebContents* web_contents) {
  NOTIMPLEMENTED();
  return NULL;
}

void ExtensionTabUtil::OpenOptionsPage(const Extension* extension,
                                       Browser* browser) {
   NOTIMPLEMENTED();
}

}  // namespace extensions
