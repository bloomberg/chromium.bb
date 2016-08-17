// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/multilanguage_options_browsertest.h"

#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"

MultilanguageOptionsBrowserTest::MultilanguageOptionsBrowserTest() {
}

MultilanguageOptionsBrowserTest::~MultilanguageOptionsBrowserTest() {
}

void MultilanguageOptionsBrowserTest::ClearSpellcheckDictionaries() {
  SetDictionariesPref(base::ListValue());
}

void MultilanguageOptionsBrowserTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();
#if defined(OS_CHROMEOS)
  std::string setting_name = prefs::kLanguagePreferredLanguages;
#else
  std::string setting_name = prefs::kAcceptLanguages;
#endif

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetString(setting_name, "fr,es,de,en");
  base::ListValue dictionaries;
  dictionaries.AppendString("fr");
  SetDictionariesPref(dictionaries);
  pref_service->SetString(spellcheck::prefs::kSpellCheckDictionary,
                          std::string());
}

void MultilanguageOptionsBrowserTest::SetDictionariesPref(
    const base::ListValue& value) {
  browser()->profile()->GetPrefs()->Set(
      spellcheck::prefs::kSpellCheckDictionaries, value);
}
