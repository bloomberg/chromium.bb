// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

namespace keys = extensions::tabs_constants;

using content::NavigationEntry;
using content::WebContents;
using extensions::APIPermission;
using extensions::Extension;

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  return browser->session_id().id();
}

int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    if ((*it)->tab_strip_model() == tab_strip_model)
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
    const Extension* extension) {
  // Find the tab strip and index of this guy.
  TabStripModel* tab_strip = NULL;
  int tab_index;
  if (ExtensionTabUtil::GetTabStripModel(contents, &tab_strip, &tab_index)) {
    return ExtensionTabUtil::CreateTabValue(contents,
                                            tab_strip,
                                            tab_index,
                                            extension);
  }

  // Couldn't find it.  This can happen if the tab is being dragged.
  return ExtensionTabUtil::CreateTabValue(contents, NULL, -1, extension);
}

ListValue* ExtensionTabUtil::CreateTabList(
    const Browser* browser,
    const Extension* extension) {
  ListValue* tab_list = new ListValue();
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_list->Append(CreateTabValue(
        tab_strip->GetTabContentsAt(i)->web_contents(),
        tab_strip,
        i,
        extension));
  }

  return tab_list;
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(
    const WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index,
    const Extension* extension) {
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

  // Only add privacy-sensitive data if the requesting extension has the tabs
  // permission.
  bool has_permission = false;
  if (extension) {
    if (tab_index >= 0) {
      has_permission =
          extension->HasAPIPermissionForTab(
              tab_index, APIPermission::kTab) ||
          extension->HasAPIPermissionForTab(
              tab_index, APIPermission::kWebNavigation);
    } else {
      has_permission =
          extension->HasAPIPermission(APIPermission::kTab) ||
          extension->HasAPIPermission(APIPermission::kWebNavigation);
    }
  }

  if (has_permission) {
    result->SetString(keys::kUrlKey, contents->GetURL().spec());
    result->SetString(keys::kTitleKey, contents->GetTitle());
    if (!is_loading) {
      NavigationEntry* entry = contents->GetController().GetActiveEntry();
      if (entry && entry->GetFavicon().valid)
        result->SetString(keys::kFaviconUrlKey, entry->GetFavicon().url.spec());
    }
  }

  if (tab_strip) {
    content::NavigationController* opener =
        tab_strip->GetOpenerOfTabContentsAt(tab_index);
    if (opener) {
      result->SetInteger(keys::kOpenerTabIdKey,
                         GetTabId(opener->GetWebContents()));
    }
  }

  return result;
}

DictionaryValue* ExtensionTabUtil::CreateTabValueActive(
    const WebContents* contents,
    bool active,
    const extensions::Extension* extension) {
  DictionaryValue* result = CreateTabValue(contents, extension);
  result->SetBoolean(keys::kSelectedKey, active);
  return result;
}

bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  DCHECK(web_contents);
  DCHECK(tab_strip_model);
  DCHECK(tab_index);

  for (BrowserList::const_iterator it = BrowserList::begin();
      it != BrowserList::end(); ++it) {
    TabStripModel* tab_strip = (*it)->tab_strip_model();
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
                                     TabContents** contents,
                                     int* tab_id) {
  DCHECK(browser);
  DCHECK(contents);

  *contents = chrome::GetActiveTabContents(browser);
  if (*contents) {
    if (tab_id)
      *tab_id = GetTabId((*contents)->web_contents());
    return true;
  }

  return false;
}

bool ExtensionTabUtil::GetTabById(int tab_id,
                                  Profile* profile,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  TabContents** contents,
                                  int* tab_index) {
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile() ?
          profile->GetOffTheRecordProfile() : NULL;
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    Browser* target_browser = *iter;
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        TabContents* target_contents = target_tab_strip->GetTabContentsAt(i);
        if (SessionID::IdForTab(target_contents->web_contents()) == tab_id) {
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
          (fixed_url.host() == chrome::kChromeUIBrowserCrashHost ||
           fixed_url.host() == chrome::kChromeUICrashHost));
}

void ExtensionTabUtil::CreateTab(WebContents* web_contents,
                                 const std::string& extension_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  Browser* browser = browser::FindTabbedBrowser(profile, false);
  const bool browser_created = !browser;
  if (!browser)
    browser = new Browser(Browser::CreateParams(profile));
  TabContents* tab_contents =
      TabContents::Factory::CreateTabContents(web_contents);
  chrome::NavigateParams params(browser, tab_contents);

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
  for (TabContentsIterator iterator; !iterator.done(); ++iterator)
    callback.Run((*iterator)->web_contents());
}

// static
extensions::WindowController* ExtensionTabUtil::GetWindowControllerOfTab(
    const WebContents* web_contents) {
  Browser* browser = browser::FindBrowserWithWebContents(web_contents);
  if (browser != NULL)
    return browser->extension_window_controller();

  return NULL;
}
