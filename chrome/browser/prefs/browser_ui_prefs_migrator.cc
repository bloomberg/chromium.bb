// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_ui_prefs_migrator.h"

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"

BrowserUIPrefsMigrator::BrowserUIPrefsMigrator(WriteablePrefStore* pref_store)
    : pref_store_(pref_store) {
}

BrowserUIPrefsMigrator::~BrowserUIPrefsMigrator() {
}

void BrowserUIPrefsMigrator::OnInitializationCompleted(
    bool succeeded) {
  pref_store_->RemoveObserver(this);
  scoped_ptr<BrowserUIPrefsMigrator> self_deleter(this);
  if (!succeeded)
    return;

  base::Value* browser_value = NULL;
  if (!pref_store_->GetMutableValue("browser", &browser_value))
    return;

  base::DictionaryValue* browser_dict = NULL;
  if (!browser_value->GetAsDictionary(&browser_dict))
    return;

  // Don't bother scanning "browser" if the migration already occurred.
  if (browser_dict->HasKey("app_window_placement"))
    return;

  // Get a set of keys in the dictionary. This must be done separately from the
  // migration because the migration modifies the dictionary being iterated.
  std::set<std::string> keys_to_check;
  for (base::DictionaryValue::Iterator it(*browser_dict); !it.IsAtEnd();
       it.Advance()) {
    keys_to_check.insert(it.key());
  }

  scoped_ptr<base::DictionaryValue> app_window_placement;
  // Apps used to have their window placement preferences registered as
  // "browser.window_placement_$APPNAME".
  const std::string search_for =
      std::string(prefs::kBrowserWindowPlacement) + "_";
  for (std::set<std::string>::const_iterator it = keys_to_check.begin();
       it != keys_to_check.end();
       ++it) {
    std::string full_key("browser." + *it);
    if (StartsWithASCII(full_key, search_for, true /* case_sensitive */)) {
      if (full_key == prefs::kBrowserWindowPlacementPopup)
        continue;
      scoped_ptr<base::Value> single_app_placement_dict;
      bool found = browser_dict->Remove(*it, &single_app_placement_dict);
      DCHECK(found);
      std::string new_key(full_key.substr(search_for.length()));
      if (!app_window_placement)
        app_window_placement.reset(new base::DictionaryValue);
      app_window_placement->Set(new_key, single_app_placement_dict.release());
    }
  }
  if (app_window_placement) {
    pref_store_->SetValue(prefs::kAppWindowPlacement,
                          app_window_placement.release());
  }
}
