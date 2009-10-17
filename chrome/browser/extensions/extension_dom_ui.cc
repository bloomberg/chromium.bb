// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_dom_ui.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"

namespace {
const wchar_t kExtensionURLOverrides[] = L"extensions.chrome_url_overrides";
}

ExtensionDOMUI::ExtensionDOMUI(TabContents* tab_contents)
    : DOMUI(tab_contents) {
  // TODO(aa): It would be cool to show the extension's icon in here.
  hide_favicon_ = true;
  should_hide_url_ = true;
  bindings_ = BindingsPolicy::EXTENSION;

  // For chrome:// overrides, some of the defaults are a little different.
  GURL url = tab_contents->GetURL();
  if (url.SchemeIs(chrome::kChromeUIScheme)) {
    if (url.host() == chrome::kChromeUINewTabHost) {
      focus_location_bar_by_default_ = true;
    } else {
      // Current behavior of other chrome:// pages is to display the URL.
      should_hide_url_ = false;
    }
  }
}

void ExtensionDOMUI::ResetExtensionFunctionDispatcher(
    RenderViewHost* render_view_host) {
  // Use the NavigationController to get the URL rather than the TabContents
  // since this is the real underlying URL (see HandleChromeURLOverride).
  NavigationController& controller = tab_contents()->controller();
  const GURL& url = controller.GetActiveEntry()->url();
  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(render_view_host, this, url));
}

void ExtensionDOMUI::RenderViewCreated(RenderViewHost* render_view_host) {
  ResetExtensionFunctionDispatcher(render_view_host);
}

void ExtensionDOMUI::RenderViewReused(RenderViewHost* render_view_host) {
  ResetExtensionFunctionDispatcher(render_view_host);
}

void ExtensionDOMUI::ProcessDOMUIMessage(const std::string& message,
                                         const Value* content,
                                         int request_id,
                                         bool has_callback) {
  extension_function_dispatcher_->HandleRequest(message, content, request_id,
                                                has_callback);
}

Browser* ExtensionDOMUI::GetBrowser() {
  return static_cast<Browser*>(tab_contents()->delegate());
}

////////////////////////////////////////////////////////////////////////////////
// chrome:// URL overrides

// static
void ExtensionDOMUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(kExtensionURLOverrides);
}

// static
bool ExtensionDOMUI::HandleChromeURLOverride(GURL* url, Profile* profile) {
  if (!url->SchemeIs(chrome::kChromeUIScheme))
    return false;

  // Even when the extensions service is enabled by default, it's still
  // disabled in incognito mode.
  ExtensionsService* service = profile->GetExtensionsService();
  if (!service)
    return false;

  const DictionaryValue* overrides =
      profile->GetPrefs()->GetDictionary(kExtensionURLOverrides);
  std::string page = url->host();
  ListValue* url_list;
  if (!overrides || !overrides->GetList(UTF8ToWide(page), &url_list))
    return false;

  if (!service->is_ready()) {
    // TODO(erikkay) So far, it looks like extensions load before the new tab
    // page.  I don't know if we have anything that enforces this, so add this
    // check for safety.
    NOTREACHED() << "Chrome URL override requested before extensions loaded";
    return false;
  }

  while (url_list->GetSize()) {
    Value* val;
    url_list->Get(0, &val);

    // Verify that the override value is good.  If not, unregister it and find
    // the next one.
    std::string override;
    if (!val->GetAsString(&override)) {
      NOTREACHED();
      UnregisterChromeURLOverride(page, profile, val);
      continue;
    }
    GURL extension_url(override);
    if (!extension_url.is_valid()) {
      NOTREACHED();
      UnregisterChromeURLOverride(page, profile, val);
      continue;
    }

    // Verify that the extension that's being referred to actually exists.
    Extension* extension = service->GetExtensionByURL(extension_url);
    if (!extension) {
      // This can currently happen if you use --load-extension one run, and
      // then don't use it the next.  It could also happen if an extension
      // were deleted directly from the filesystem, etc.
      LOG(WARNING) << "chrome URL override present for non-existant extension";
      UnregisterChromeURLOverride(page, profile, val);
      continue;
    }

    *url = extension_url;
    return true;
  }
  return false;
}

// static
void ExtensionDOMUI::RegisterChromeURLOverrides(
    Profile* profile, const Extension::URLOverrideMap& overrides) {
  if (overrides.empty())
    return;

  PrefService* prefs = profile->GetPrefs();
  DictionaryValue* all_overrides =
      prefs->GetMutableDictionary(kExtensionURLOverrides);

  // For each override provided by the extension, add it to the front of
  // the override list if it's not already in the list.
  Extension::URLOverrideMap::const_iterator iter = overrides.begin();
  for (;iter != overrides.end(); ++iter) {
    const std::wstring key = UTF8ToWide((*iter).first);
    ListValue* page_overrides;
    if (!all_overrides->GetList(key, &page_overrides)) {
      page_overrides = new ListValue();
      all_overrides->Set(key, page_overrides);
    } else {
      // Verify that the override isn't already in the list.
      ListValue::iterator i = page_overrides->begin();
      for (; i != page_overrides->end(); ++i) {
        std::string override_val;
        if (!(*i)->GetAsString(&override_val)) {
          NOTREACHED();
          continue;
        }
        if (override_val == (*iter).first)
          break;
      }
      // This value is already in the list, leave it alone.
      if (i != page_overrides->end())
        continue;
    }
    // Insert the override at the front of the list.  Last registered override
    // wins.
    page_overrides->Insert(0, new StringValue((*iter).second.spec()));
  }
}

// static
void ExtensionDOMUI::UnregisterAndReplaceOverride(const std::string& page,
    Profile* profile, ListValue* list, Value* override) {
  int index = list->Remove(*override);
  if (index == 0) {
    // This is the active override, so we need to find all existing
    // tabs for this override and get them to reload the original URL.
    for (TabContentsIterator iterator; !iterator.done(); ++iterator) {
      TabContents* tab = *iterator;
      if (tab->profile() != profile)
        continue;

      GURL url = tab->GetURL();
      if (!url.SchemeIs(chrome::kChromeUIScheme) || url.host() != page)
        continue;

      // Don't use Reload() since |url| isn't the same as the internal URL
      // that NavigationController has.
      tab->controller().LoadURL(url, url, PageTransition::RELOAD);
    }
  }
}

// static
void ExtensionDOMUI::UnregisterChromeURLOverride(const std::string& page,
    Profile* profile, Value* override) {
  if (!override)
    return;
  PrefService* prefs = profile->GetPrefs();
  DictionaryValue* all_overrides =
      prefs->GetMutableDictionary(kExtensionURLOverrides);
  ListValue* page_overrides;
  if (!all_overrides->GetList(UTF8ToWide(page), &page_overrides)) {
    // If it's being unregistered, it should already be in the list.
    NOTREACHED();
    return;
  } else {
    UnregisterAndReplaceOverride(page, profile, page_overrides, override);
  }
}

// static
void ExtensionDOMUI::UnregisterChromeURLOverrides(
    Profile* profile, const Extension::URLOverrideMap& overrides) {
  if (overrides.empty())
    return;
  PrefService* prefs = profile->GetPrefs();
  DictionaryValue* all_overrides =
      prefs->GetMutableDictionary(kExtensionURLOverrides);
  Extension::URLOverrideMap::const_iterator iter = overrides.begin();
  for (;iter != overrides.end(); ++iter) {
    std::wstring page = UTF8ToWide((*iter).first);
    ListValue* page_overrides;
    if (!all_overrides->GetList(page, &page_overrides)) {
      // If it's being unregistered, it should already be in the list.
      NOTREACHED();
      continue;
    } else {
      StringValue override((*iter).second.spec());
      UnregisterAndReplaceOverride((*iter).first, profile,
                                   page_overrides, &override);
    }
  }
}
