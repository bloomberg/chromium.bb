// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language_combobox_model.h"

#include "base/i18n/rtl.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "unicode/uloc.h"

///////////////////////////////////////////////////////////////////////////////
// LanguageList used to enumerate native names corresponding to the
// language code (e.g. English (United States) for en-US)
//

LanguageList::LanguageList() {
  // Enumerate the languages we know about.
  const std::vector<std::string>& locale_codes =
      l10n_util::GetAvailableLocales();
  InitNativeNames(locale_codes);
}

LanguageList::LanguageList(
    const std::vector<std::string>& locale_codes) {
  InitNativeNames(locale_codes);
}

LanguageList::~LanguageList() {}

void LanguageList::InitNativeNames(
    const std::vector<std::string>& locale_codes) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < locale_codes.size(); ++i) {
    std::string locale_code_str = locale_codes[i];
    const char* locale_code = locale_codes[i].c_str();

    // TODO(jungshik): Even though these strings are used for the UI,
    // the old code does not add an RTL mark for RTL locales. Make sure
    // that it's ok without that.
    string16 name_in_current_ui =
        l10n_util::GetDisplayNameForLocale(locale_code, app_locale, false);
    string16 name_native =
        l10n_util::GetDisplayNameForLocale(locale_code, locale_code, false);

    locale_names_.push_back(name_in_current_ui);
    native_names_[name_in_current_ui] =
        LocaleData(name_native, locale_codes[i]);
  }

  // Sort using locale specific sorter.
  l10n_util::SortStrings16(g_browser_process->GetApplicationLocale(),
                           &locale_names_);
}

void LanguageList::CopySpecifiedLanguagesUp(const std::string& locale_codes) {
  DCHECK(!locale_names_.empty());
  std::vector<std::string> locale_codes_vector;
  base::SplitString(locale_codes, ',', &locale_codes_vector);
  for (size_t i = 0; i != locale_codes_vector.size(); i++) {
    const int locale_index = GetIndexFromLocale(locale_codes_vector[i]);
    CHECK_NE(locale_index, -1);
    locale_names_.insert(locale_names_.begin(), locale_names_[locale_index]);
  }
}

// Overridden from ComboboxModel:
int LanguageList::get_languages_count() const {
  return static_cast<int>(locale_names_.size());
}

string16 LanguageList::GetLanguageNameAt(int index) const {
  DCHECK(static_cast<int>(locale_names_.size()) > index);
  LocaleDataMap::const_iterator it =
      native_names_.find(locale_names_[index]);
  DCHECK(it != native_names_.end());

  // If the name is the same in the native language and local language,
  // don't show it twice.
  if (it->second.native_name == locale_names_[index])
    return it->second.native_name;

  // We must add directionality formatting to both the native name and the
  // locale name in order to avoid text rendering problems such as misplaced
  // parentheses or languages appearing in the wrong order.
  string16 locale_name = locale_names_[index];
  base::i18n::AdjustStringForLocaleDirection(&locale_name);

  string16 native_name = it->second.native_name;
  base::i18n::AdjustStringForLocaleDirection(&native_name);

  // We used to have a localizable template here, but none of translators
  // changed the format. We also want to switch the order of locale_name
  // and native_name without going back to translators.
  std::string formatted_item;
  base::SStringPrintf(&formatted_item, "%s - %s",
                      UTF16ToUTF8(locale_name).c_str(),
                      UTF16ToUTF8(native_name).c_str());
  if (base::i18n::IsRTL())
    // Somehow combo box (even with LAYOUTRTL flag) doesn't get this
    // right so we add RTL BDO (U+202E) to set the direction
    // explicitly.
    formatted_item.insert(0, "\xE2\x80\xAE");  // U+202E = UTF-8 0xE280AE
  return UTF8ToUTF16(formatted_item);
}

// Return the locale for the given index.  E.g., may return pt-BR.
std::string LanguageList::GetLocaleFromIndex(int index) const {
  DCHECK(static_cast<int>(locale_names_.size()) > index);
  LocaleDataMap::const_iterator it =
      native_names_.find(locale_names_[index]);
  DCHECK(it != native_names_.end());

  return it->second.locale_code;
}

int LanguageList::GetIndexFromLocale(const std::string& locale) const {
  for (size_t i = 0; i < locale_names_.size(); ++i) {
    LocaleDataMap::const_iterator it =
        native_names_.find(locale_names_[i]);
    DCHECK(it != native_names_.end());
    if (it->second.locale_code == locale)
      return static_cast<int>(i);
  }
  return -1;
}

///////////////////////////////////////////////////////////////////////////////
// LanguageComboboxModel used to populate a combobox with native names
//

LanguageComboboxModel::LanguageComboboxModel()
    : profile_(NULL) {
}

LanguageComboboxModel::LanguageComboboxModel(
    Profile* profile, const std::vector<std::string>& locale_codes)
    : LanguageList(locale_codes),
      profile_(profile) {
}

LanguageComboboxModel::~LanguageComboboxModel() {}

int LanguageComboboxModel::GetItemCount() {
  return get_languages_count();
}

string16 LanguageComboboxModel::GetItemAt(int index) {
  return GetLanguageNameAt(index);
}

// Returns the index of the language currently specified in the user's
// preference file.  Note that it's possible for language A to be picked
// while chrome is currently in language B if the user specified language B
// via --lang.  Since --lang is not a persistent setting, it seems that it
// shouldn't be reflected in this combo box.  We return -1 if the value in
// the pref doesn't map to a know language (possible if the user edited the
// prefs file manually).
int LanguageComboboxModel::GetSelectedLanguageIndex(const std::string& prefs) {
  PrefService* local_state;
  if (!profile_)
    local_state = g_browser_process->local_state();
  else
    local_state = profile_->GetPrefs();

  DCHECK(local_state);
  const std::string& current_locale = local_state->GetString(prefs.c_str());

  return GetIndexFromLocale(current_locale);
}
