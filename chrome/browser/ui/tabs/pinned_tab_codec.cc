// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_codec.h"

#include "base/values.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::NavigationEntry;

// Key used in dictionaries for the app id.
static const char kAppID[] = "app_id";

// Key used in dictionaries for the url.
static const char kURL[] = "url";

// Returns true if |browser| has any pinned tabs.
static bool HasPinnedTabs(Browser* browser) {
  TabStripModel* tab_model = browser->tab_strip_model();
  for (int i = 0; i < tab_model->count(); ++i) {
    if (tab_model->IsTabPinned(i))
      return true;
  }
  return false;
}

// Adds a DictionaryValue to |values| representing |tab|.
static void EncodeTab(const StartupTab& tab, ListValue* values) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue);
  value->SetString(kURL, tab.url.spec());
  if (tab.is_app)
    value->SetString(kAppID, tab.app_id);
  values->Append(value.release());
}

// Adds a DictionaryValue to |values| representing the pinned tab at the
// specified index.
static void EncodePinnedTab(TabStripModel* model,
                            int index,
                            ListValue* values) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());

  content::WebContents* web_contents =
      model->GetTabContentsAt(index)->web_contents();
  if (model->IsAppTab(index)) {
    const extensions::Extension* extension =
        extensions::TabHelper::FromWebContents(web_contents)->extension_app();
    DCHECK(extension);
    value->SetString(kAppID, extension->id());
    // For apps we use the launch url. We do this because the user is
    // effectively restarting the app, so returning them to the app's launch
    // page seems closest to what they expect.
    value->SetString(kURL, extension->GetFullLaunchURL().spec());
    values->Append(value.release());
  } else {
    NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
    if (!entry && web_contents->GetController().GetEntryCount())
      entry = web_contents->GetController().GetEntryAtIndex(0);
    if (entry) {
      value->SetString(kURL, entry->GetURL().spec());
      values->Append(value.release());
    }
  }
}

// Invokes EncodePinnedTab for each pinned tab in browser.
static void EncodePinnedTabs(Browser* browser, ListValue* values) {
  TabStripModel* tab_model = browser->tab_strip_model();
  for (int i = 0; i < tab_model->count() && tab_model->IsTabPinned(i); ++i)
    EncodePinnedTab(tab_model, i, values);
}

// Decodes the previously written values in |value| to |tab|, returning true
// on success.
static bool DecodeTab(const DictionaryValue& value, StartupTab* tab) {
  tab->is_app = false;

  std::string url_string;
  if (!value.GetString(kURL, &url_string))
    return false;
  tab->url = GURL(url_string);

  if (value.GetString(kAppID, &(tab->app_id)))
    tab->is_app = true;

  return true;
}

// static
void PinnedTabCodec::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kPinnedTabs, PrefService::UNSYNCABLE_PREF);
}

// static
void PinnedTabCodec::WritePinnedTabs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return;

  ListValue values;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    Browser* browser = *i;
    if (browser->is_type_tabbed() &&
        browser->profile() == profile && HasPinnedTabs(browser)) {
      EncodePinnedTabs(browser, &values);
    }
  }
  prefs->Set(prefs::kPinnedTabs, values);
}

// static
void PinnedTabCodec::WritePinnedTabs(Profile* profile,
                                     const StartupTabs& tabs) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return;

  ListPrefUpdate update(prefs, prefs::kPinnedTabs);
  ListValue* values = update.Get();
  values->Clear();
  for (StartupTabs::const_iterator i = tabs.begin(); i != tabs.end(); ++i)
    EncodeTab(*i, values);
}

// static
StartupTabs PinnedTabCodec::ReadPinnedTabs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return StartupTabs();
  return ReadPinnedTabs(prefs->GetList(prefs::kPinnedTabs));
}

// static
StartupTabs PinnedTabCodec::ReadPinnedTabs(const base::Value* value) {
  StartupTabs results;

  const base::ListValue* tabs_list = NULL;
  if (!value->GetAsList(&tabs_list))
    return results;

  for (size_t i = 0, max = tabs_list->GetSize(); i < max; ++i) {
    const base::DictionaryValue* tab_values = NULL;
    if (tabs_list->GetDictionary(i, &tab_values)) {
      StartupTab tab;
      if (DecodeTab(*tab_values, &tab))
        results.push_back(tab);
    }
  }
  return results;
}
