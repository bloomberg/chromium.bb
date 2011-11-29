// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/per_tab_prefs_tab_helper.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/webpreferences.h"

const char* kPerTabPrefsToObserve[] = {
    prefs::kWebKitJavascriptEnabled
};

const int kPerTabPrefsToObserveLength = arraysize(kPerTabPrefsToObserve);

PerTabPrefsTabHelper::PerTabPrefsTabHelper(
    TabContentsWrapper* wrapper)
    : TabContentsObserver(wrapper->tab_contents()),
      wrapper_(wrapper) {
  prefs_.reset(
      wrapper_->profile()->GetPrefs()->CreatePrefServiceWithPerTabPrefStore());
  RegisterPerTabUserPrefs(prefs_.get());

  // Notify the wrapper about all interested prefs changes.
  pref_change_registrar_.Init(prefs_.get());
  for (int i = 0; i < kPerTabPrefsToObserveLength; ++i) {
      pref_change_registrar_.Add(kPerTabPrefsToObserve[i], wrapper_);
  }
}

PerTabPrefsTabHelper::~PerTabPrefsTabHelper() {
}

void PerTabPrefsTabHelper::OverrideWebPreferences(WebPreferences* prefs) {
  prefs->javascript_enabled =
      prefs_->GetBoolean(prefs::kWebKitJavascriptEnabled);
}

void PerTabPrefsTabHelper::RegisterPerTabUserPrefs(PrefService* prefs) {
  WebPreferences pref_defaults;

  prefs->RegisterBooleanPref(prefs::kWebKitJavascriptEnabled,
                             pref_defaults.javascript_enabled,
                             PrefService::UNSYNCABLE_PREF);
}

void PerTabPrefsTabHelper::TabContentsDestroyed(TabContents* tab) {
  pref_change_registrar_.RemoveAll();
}
