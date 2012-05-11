// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

namespace keys = extension_tabs_module_constants;
namespace errors = extension_manifest_errors;

using content::NavigationEntry;
using content::WebContents;

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  return browser->session_id().id();
}

int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    if ((*it)->tabstrip_model() == tab_strip_model)
      return GetWindowId(*it);
  }
  return -1;
}

// TODO(sky): this function should really take a TabContentsWrapper.
int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  const TabContentsWrapper* tab =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents);
  return tab ? tab->restore_tab_helper()->session_id().id() : -1;
}

std::string ExtensionTabUtil::GetTabStatusText(bool is_loading) {
  return is_loading ? keys::kStatusValueLoading : keys::kStatusValueComplete;
}

// TODO(sky): this function should really take a TabContentsWrapper.
int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  const TabContentsWrapper* tab =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents);
  return tab ? tab->restore_tab_helper()->window_id().id() : -1;
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(const WebContents* contents) {
  // Find the tab strip and index of this guy.
  TabStripModel* tab_strip = NULL;
  int tab_index;
  if (ExtensionTabUtil::GetTabStripModel(contents, &tab_strip, &tab_index))
    return ExtensionTabUtil::CreateTabValue(contents, tab_strip, tab_index);

  // Couldn't find it.  This can happen if the tab is being dragged.
  return ExtensionTabUtil::CreateTabValue(contents, NULL, -1);
}

ListValue* ExtensionTabUtil::CreateTabList(const Browser* browser) {
  ListValue* tab_list = new ListValue();
  TabStripModel* tab_strip = browser->tabstrip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_list->Append(ExtensionTabUtil::CreateTabValue(
        tab_strip->GetTabContentsAt(i)->web_contents(), tab_strip, i));
  }

  return tab_list;
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(const WebContents* contents,
                                                  TabStripModel* tab_strip,
                                                  int tab_index) {
  DictionaryValue* result = new DictionaryValue();
  bool is_loading = contents->IsLoading();
  result->SetInteger(keys::kIdKey, ExtensionTabUtil::GetTabId(contents));
  result->SetInteger(keys::kIndexKey, tab_index);
  result->SetInteger(keys::kWindowIdKey,
                     ExtensionTabUtil::GetWindowIdOfTab(contents));
  result->SetString(keys::kUrlKey, contents->GetURL().spec());
  result->SetString(keys::kStatusKey, GetTabStatusText(is_loading));
  result->SetBoolean(keys::kActiveKey,
                     tab_strip && tab_index == tab_strip->active_index());
  result->SetBoolean(keys::kSelectedKey,
                     tab_strip && tab_index == tab_strip->active_index());
  result->SetBoolean(keys::kHighlightedKey,
                   tab_strip && tab_strip->IsTabSelected(tab_index));
  result->SetBoolean(keys::kPinnedKey,
                     tab_strip && tab_strip->IsTabPinned(tab_index));
  result->SetString(keys::kTitleKey, contents->GetTitle());
  result->SetBoolean(keys::kIncognitoKey,
                     contents->GetBrowserContext()->IsOffTheRecord());

  if (tab_strip) {
    content::NavigationController* opener =
        tab_strip->GetOpenerOfTabContentsAt(tab_index);
    if (opener) {
      result->SetInteger(keys::kOpenerTabIdKey,
                         ExtensionTabUtil::GetTabId(opener->GetWebContents()));
    }
  }

  if (!is_loading) {
    NavigationEntry* entry = contents->GetController().GetActiveEntry();
    if (entry) {
      if (entry->GetFavicon().valid)
        result->SetString(keys::kFaviconUrlKey, entry->GetFavicon().url.spec());
    }
  }

  return result;
}

DictionaryValue* ExtensionTabUtil::CreateTabValueActive(
    const WebContents* contents,
    bool active) {
  DictionaryValue* result = ExtensionTabUtil::CreateTabValue(contents);
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
    TabStripModel* tab_strip = (*it)->tabstrip_model();
    int index = tab_strip->GetWrapperIndex(web_contents);
    if (index != -1) {
      *tab_strip_model = tab_strip;
      *tab_index = index;
      return true;
    }
  }

  return false;
}

bool ExtensionTabUtil::GetDefaultTab(Browser* browser,
                                     TabContentsWrapper** contents,
                                     int* tab_id) {
  DCHECK(browser);
  DCHECK(contents);

  *contents = browser->GetSelectedTabContentsWrapper();
  if (*contents) {
    if (tab_id)
      *tab_id = ExtensionTabUtil::GetTabId((*contents)->web_contents());
    return true;
  }

  return false;
}

bool ExtensionTabUtil::GetTabById(int tab_id,
                                  Profile* profile,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  TabContentsWrapper** contents,
                                  int* tab_index) {
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile() ?
          profile->GetOffTheRecordProfile() : NULL;
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    Browser* target_browser = *iter;
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tabstrip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        TabContentsWrapper* target_contents =
            target_tab_strip->GetTabContentsAt(i);
        if (target_contents->restore_tab_helper()->session_id().id() ==
            tab_id) {
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
                                                  const Extension* extension) {
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
