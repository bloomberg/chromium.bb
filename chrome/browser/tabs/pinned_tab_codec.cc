// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/pinned_tab_codec.h"

#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"

typedef BrowserInit::LaunchWithProfile::Tab Tab;

// Key used in dictionaries for the app id.
static const char kAppID[] = "app_id";

// Key used in dictionaries for the url.
static const char kURL[] = "url";

// Returns true if |browser| has any pinned tabs.
static bool HasPinnedTabs(Browser* browser) {
  TabStripModel* tab_model = browser->tabstrip_model();
  for (int i = 0; i < tab_model->count(); ++i) {
    if (tab_model->IsTabPinned(i))
      return true;
  }
  return false;
}

// Adds a DictionaryValue to |values| representing the pinned tab at the
// specified index.
static void EncodePinnedTab(TabStripModel* model,
                            int index,
                            ListValue* values) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());

  TabContentsWrapper* tab_contents = model->GetTabContentsAt(index);
  if (model->IsAppTab(index)) {
    const Extension* extension = tab_contents->extension_app();
    DCHECK(extension);
    value->SetString(kAppID, extension->id());
    // For apps we use the launch url. We do this for the following reason:
    // . the user is effectively restarting the app, so that returning them to
    //   the app's launch page seems closest to what they expect.
    value->SetString(kURL, extension->GetFullLaunchURL().spec());
    values->Append(value.release());
  } else {
    NavigationEntry* entry = tab_contents->controller().GetActiveEntry();
    if (!entry && tab_contents->controller().entry_count())
      entry = tab_contents->controller().GetEntryAtIndex(0);
    if (entry) {
      value->SetString(kURL, entry->url().spec());
      values->Append(value.release());
    }
  }
}

// Invokes EncodePinnedTab for each pinned tab in browser.
static void EncodePinnedTabs(Browser* browser, ListValue* values) {
  TabStripModel* tab_model = browser->tabstrip_model();
  for (int i = 0; i < tab_model->count() && tab_model->IsTabPinned(i); ++i)
    EncodePinnedTab(tab_model, i, values);
}

// Decodes the previously written values in |value| to |tab|, returning true
// on success.
static bool DecodeTab(const DictionaryValue& value, Tab* tab) {
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
  prefs->RegisterListPref(prefs::kPinnedTabs);
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
    if (browser->type() == Browser::TYPE_NORMAL &&
        browser->profile() == profile && HasPinnedTabs(browser)) {
      EncodePinnedTabs(browser, &values);
    }
  }
  prefs->Set(prefs::kPinnedTabs, values);
  prefs->ScheduleSavePersistentPrefs();
}

// static
std::vector<Tab> PinnedTabCodec::ReadPinnedTabs(Profile* profile) {
  std::vector<Tab> results;

  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return results;

  const ListValue* pref_value = prefs->GetList(prefs::kPinnedTabs);
  if (!pref_value)
    return results;

  for (size_t i = 0, max = pref_value->GetSize(); i < max; ++i) {
    DictionaryValue* values = NULL;
    if (pref_value->GetDictionary(i, &values)) {
      Tab tab;
      if (DecodeTab(*values, &tab))
        results.push_back(tab);
    }
  }
  return results;
}
