// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"

namespace keys = extension_tabs_module_constants;
namespace errors = extension_manifest_errors;

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
int ExtensionTabUtil::GetTabId(const TabContents* tab_contents) {
  const TabContentsWrapper* tab =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  return tab ? tab->restore_tab_helper()->session_id().id() : -1;
}

std::string ExtensionTabUtil::GetTabStatusText(bool is_loading) {
  return is_loading ? keys::kStatusValueLoading : keys::kStatusValueComplete;
}

// TODO(sky): this function should really take a TabContentsWrapper.
int ExtensionTabUtil::GetWindowIdOfTab(const TabContents* tab_contents) {
  const TabContentsWrapper* tab =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  return tab ? tab->restore_tab_helper()->window_id().id() : -1;
}

// Return the type name for a browser window type.
std::string ExtensionTabUtil::GetWindowTypeText(const Browser* browser) {
  if (browser->is_type_popup())
    return keys::kWindowTypeValuePopup;
  if (browser->is_type_panel())
    return keys::kWindowTypeValuePanel;
  if (browser->is_app())
    return keys::kWindowTypeValueApp;
  return keys::kWindowTypeValueNormal;
}

// Return the state name for a browser window state.
std::string ExtensionTabUtil::GetWindowShowStateText(const Browser* browser) {
  BrowserWindow* window = browser->window();
  if (window->IsMinimized())
    return keys::kShowStateValueMinimized;
  if (window->IsMaximized() || window->IsFullscreen())
    return keys::kShowStateValueMaximized;
  return keys::kShowStateValueNormal;
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(
    const TabContents* contents) {
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
        tab_strip->GetTabContentsAt(i)->tab_contents(), tab_strip, i));
  }

  return tab_list;
}

DictionaryValue* ExtensionTabUtil::CreateTabValue(const TabContents* contents,
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
                     contents->browser_context()->IsOffTheRecord());

  if (!is_loading) {
    NavigationEntry* entry = contents->controller().GetActiveEntry();
    if (entry) {
      if (entry->favicon().is_valid())
        result->SetString(keys::kFaviconUrlKey, entry->favicon().url().spec());
    }
  }

  return result;
}

DictionaryValue* ExtensionTabUtil::CreateTabValueActive(
    const TabContents* contents,
    bool active) {
  DictionaryValue* result = ExtensionTabUtil::CreateTabValue(contents);
  result->SetBoolean(keys::kSelectedKey, active);
  return result;
}

// if |populate| is true, each window gets a list property |tabs| which contains
// fully populated tab objects.
DictionaryValue* ExtensionTabUtil::CreateWindowValue(const Browser* browser,
                                                     bool populate_tabs) {
  DCHECK(browser);
  DCHECK(browser->window());
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(keys::kIdKey, ExtensionTabUtil::GetWindowId(browser));
  result->SetBoolean(keys::kIncognitoKey,
                     browser->profile()->IsOffTheRecord());
  result->SetBoolean(keys::kFocusedKey, browser->window()->IsActive());
  gfx::Rect bounds;
  if (browser->window()->IsMaximized() || browser->window()->IsFullscreen())
    bounds = browser->window()->GetBounds();
  else
    bounds = browser->window()->GetRestoredBounds();

  result->SetInteger(keys::kLeftKey, bounds.x());
  result->SetInteger(keys::kTopKey, bounds.y());
  result->SetInteger(keys::kWidthKey, bounds.width());
  result->SetInteger(keys::kHeightKey, bounds.height());
  result->SetString(keys::kWindowTypeKey, GetWindowTypeText(browser));
  result->SetString(keys::kShowStateKey, GetWindowShowStateText(browser));

  if (populate_tabs) {
    result->Set(keys::kTabsKey, ExtensionTabUtil::CreateTabList(browser));
  }

  return result;
}

bool ExtensionTabUtil::GetTabStripModel(const TabContents* tab_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  DCHECK(tab_contents);
  DCHECK(tab_strip_model);
  DCHECK(tab_index);

  for (BrowserList::const_iterator it = BrowserList::begin();
      it != BrowserList::end(); ++it) {
    TabStripModel* tab_strip = (*it)->tabstrip_model();
    int index = tab_strip->GetWrapperIndex(tab_contents);
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
      *tab_id = ExtensionTabUtil::GetTabId((*contents)->tab_contents());
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
