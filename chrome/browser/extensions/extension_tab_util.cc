// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

namespace keys = extensions::tabs_constants;
namespace tabs = extensions::api::tabs;

using content::NavigationEntry;
using content::WebContents;
using extensions::APIPermission;
using extensions::Extension;

namespace {

extensions::WindowController* GetShellWindowController(
    const WebContents* contents) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  extensions::ShellWindowRegistry* registry =
      extensions::ShellWindowRegistry::Get(profile);
  if (!registry)
    return NULL;
  ShellWindow* shell_window =
      registry->GetShellWindowForRenderViewHost(contents->GetRenderViewHost());
  if (!shell_window)
    return NULL;
  return extensions::WindowControllerList::GetInstance()->
      FindWindowById(shell_window->session_id().id());
}

}  // namespace

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  return browser->session_id().id();
}

int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (it->tab_strip_model() == tab_strip_model)
      return GetWindowId(*it);
  }
  return -1;
}

int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  return SessionID::IdForTab(web_contents);
}

std::string ExtensionTabUtil::GetTabStatusText(bool is_loading) {
  return is_loading ? keys::kStatusValueLoading : keys::kStatusValueComplete;
}

int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  return SessionID::IdForWindowContainingTab(web_contents);
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(
    const WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index,
    const Extension* extension) {
  // If we have a matching ShellWindow with a controller, get the tab value
  // from its controller instead.
  extensions::WindowController* controller = GetShellWindowController(contents);
  if (controller &&
      (!extension || controller->IsVisibleToExtension(extension))) {
    return controller->CreateTabValue(extension, tab_index);
  }
  DictionaryValue *result = CreateTabValue(contents, tab_strip, tab_index);
  ScrubTabValueForExtension(contents, extension, result);
  return result;
}

ListValue* ExtensionTabUtil::CreateTabList(
    const Browser* browser,
    const Extension* extension) {
  ListValue* tab_list = new ListValue();
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_list->Append(CreateTabValue(tab_strip->GetWebContentsAt(i),
                                    tab_strip,
                                    i,
                                    extension));
  }

  return tab_list;
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(
    const WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index) {
  // If we have a matching ShellWindow with a controller, get the tab value
  // from its controller instead.
  extensions::WindowController* controller = GetShellWindowController(contents);
  if (controller)
    return controller->CreateTabValue(NULL, tab_index);

  if (!tab_strip)
    ExtensionTabUtil::GetTabStripModel(contents, &tab_strip, &tab_index);

  DictionaryValue* result = new DictionaryValue();
  bool is_loading = contents->IsLoading();
  result->SetInteger(keys::kIdKey, GetTabId(contents));
  result->SetInteger(keys::kIndexKey, tab_index);
  result->SetInteger(keys::kWindowIdKey, GetWindowIdOfTab(contents));
  result->SetString(keys::kStatusKey, GetTabStatusText(is_loading));
  result->SetBoolean(keys::kActiveKey,
                     tab_strip && tab_index == tab_strip->active_index());
  result->SetBoolean(keys::kSelectedKey,
                     tab_strip && tab_index == tab_strip->active_index());
  result->SetBoolean(keys::kHighlightedKey,
                   tab_strip && tab_strip->IsTabSelected(tab_index));
  result->SetBoolean(keys::kPinnedKey,
                     tab_strip && tab_strip->IsTabPinned(tab_index));
  result->SetBoolean(keys::kIncognitoKey,
                     contents->GetBrowserContext()->IsOffTheRecord());

  // Privacy-sensitive fields: these should be stripped off by
  // ScrubTabValueForExtension if the extension should not see them.
  result->SetString(keys::kUrlKey, contents->GetURL().spec());
  result->SetString(keys::kTitleKey, contents->GetTitle());
  if (!is_loading) {
    NavigationEntry* entry = contents->GetController().GetActiveEntry();
    if (entry && entry->GetFavicon().valid)
      result->SetString(keys::kFaviconUrlKey, entry->GetFavicon().url.spec());
  }

  if (tab_strip) {
    WebContents* opener = tab_strip->GetOpenerOfWebContentsAt(tab_index);
    if (opener)
      result->SetInteger(keys::kOpenerTabIdKey, GetTabId(opener));
  }

  return result;
}

void ExtensionTabUtil::ScrubTabValueForExtension(const WebContents* contents,
                                                 const Extension* extension,
                                                 DictionaryValue* tab_info) {
  bool has_permission = extension && extension->HasAPIPermissionForTab(
      GetTabId(contents), APIPermission::kTab);

  if (!has_permission) {
    tab_info->Remove(keys::kUrlKey, NULL);
    tab_info->Remove(keys::kTitleKey, NULL);
    tab_info->Remove(keys::kFaviconUrlKey, NULL);
  }
}

void ExtensionTabUtil::ScrubTabForExtension(const Extension* extension,
                                            tabs::Tab* tab) {
  bool has_permission = extension && extension->HasAPIPermission(
      APIPermission::kTab);

  if (!has_permission) {
    tab->url.reset();
    tab->title.reset();
    tab->fav_icon_url.reset();
  }
}

bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  DCHECK(web_contents);
  DCHECK(tab_strip_model);
  DCHECK(tab_index);

  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    TabStripModel* tab_strip = it->tab_strip_model();
    int index = tab_strip->GetIndexOfWebContents(web_contents);
    if (index != -1) {
      *tab_strip_model = tab_strip;
      *tab_index = index;
      return true;
    }
  }

  return false;
}

bool ExtensionTabUtil::GetDefaultTab(Browser* browser,
                                     WebContents** contents,
                                     int* tab_id) {
  DCHECK(browser);
  DCHECK(contents);

  *contents = browser->tab_strip_model()->GetActiveWebContents();
  if (*contents) {
    if (tab_id)
      *tab_id = GetTabId(*contents);
    return true;
  }

  return false;
}

bool ExtensionTabUtil::GetTabById(int tab_id,
                                  Profile* profile,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  WebContents** contents,
                                  int* tab_index) {
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile() ?
          profile->GetOffTheRecordProfile() : NULL;
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* target_browser = *it;
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        WebContents* target_contents = target_tab_strip->GetWebContentsAt(i);
        if (SessionID::IdForTab(target_contents) == tab_id) {
          if (browser)
            *browser = target_browser;
          if (tab_strip)
            *tab_strip = target_tab_strip;
          if (contents)
            *contents = target_contents;
          if (tab_index)
            *tab_index = i;
          return true;
        }
      }
    }
  }
  return false;
}

GURL ExtensionTabUtil::ResolvePossiblyRelativeURL(const std::string& url_string,
    const extensions::Extension* extension) {
  GURL url = GURL(url_string);
  if (!url.is_valid())
    url = extension->GetResourceURL(url_string);

  return url;
}

bool ExtensionTabUtil::IsCrashURL(const GURL& url) {
  // Check a fixed-up URL, to normalize the scheme and parse hosts correctly.
  GURL fixed_url =
      URLFixerUpper::FixupURL(url.possibly_invalid_spec(), std::string());
  return (fixed_url.SchemeIs(chrome::kChromeUIScheme) &&
          (fixed_url.host() == content::kChromeUIBrowserCrashHost ||
           fixed_url.host() == chrome::kChromeUICrashHost));
}

void ExtensionTabUtil::CreateTab(WebContents* web_contents,
                                 const std::string& extension_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  chrome::HostDesktopType active_desktop = chrome::GetActiveDesktop();
  Browser* browser = chrome::FindTabbedBrowser(profile, false, active_desktop);
  const bool browser_created = !browser;
  if (!browser)
    browser = new Browser(Browser::CreateParams(profile, active_desktop));
  chrome::NavigateParams params(browser, web_contents);

  // The extension_app_id parameter ends up as app_name in the Browser
  // which causes the Browser to return true for is_app().  This affects
  // among other things, whether the location bar gets displayed.
  // TODO(mpcomplete): This seems wrong. What if the extension content is hosted
  // in a tab?
  if (disposition == NEW_POPUP)
    params.extension_app_id = extension_id;

  params.disposition = disposition;
  params.window_bounds = initial_pos;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  chrome::Navigate(&params);

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != params.browser))
    browser->window()->Close();
}

// static
void ExtensionTabUtil::ForEachTab(
    const base::Callback<void(WebContents*)>& callback) {
  for (TabContentsIterator iterator; !iterator.done(); iterator.Next())
    callback.Run(*iterator);
}

// static
extensions::WindowController* ExtensionTabUtil::GetWindowControllerOfTab(
    const WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser != NULL)
    return browser->extension_window_controller();

  return NULL;
}

void ExtensionTabUtil::OpenOptionsPage(const Extension* extension,
                                       Browser* browser) {
  DCHECK(!extensions::ManifestURL::GetOptionsPage(extension).is_empty());

  // Force the options page to open in non-OTR window, because it won't be
  // able to save settings from OTR.
  if (browser->profile()->IsOffTheRecord()) {
    browser = chrome::FindOrCreateTabbedBrowser(
        browser->profile()->GetOriginalProfile(), browser->host_desktop_type());
  }

  content::OpenURLParams params(
      extensions::ManifestURL::GetOptionsPage(extension),
      content::Referrer(), SINGLETON_TAB,
      content::PAGE_TRANSITION_LINK, false);
  browser->OpenURL(params);
  browser->window()->Show();
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
}
