// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/input_method_descriptor.h"
#include "chromeos/ime/input_method_manager.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

base::DictionaryValue* CreateInputMethodsEntry(
    const input_method::InputMethodDescriptor& method,
    const std::string selected) {
  input_method::InputMethodUtil* util =
      input_method::InputMethodManager::Get()->GetInputMethodUtil();
  const std::string& ime_id = method.id();
  scoped_ptr<base::DictionaryValue> input_method(new base::DictionaryValue);
  input_method->SetString("value", ime_id);
  input_method->SetString("title", util->GetInputMethodLongName(method));
  input_method->SetBoolean("selected", ime_id == selected);
  return input_method.release();
}

// Returns true if element was inserted.
bool InsertString(const std::string& str, std::set<std::string>& to) {
  const std::pair<std::set<std::string>::iterator, bool> result =
      to.insert(str);
  return result.second;
}

void AddOptgroupOtherLayouts(base::ListValue* input_methods_list) {
  scoped_ptr<base::DictionaryValue> optgroup(new base::DictionaryValue);
  optgroup->SetString(
      "optionGroupName",
      l10n_util::GetStringUTF16(IDS_OOBE_OTHER_KEYBOARD_LAYOUTS));
  input_methods_list->Append(optgroup.release());
}

// TODO(zork): Remove this blacklist when fonts are added to Chrome OS.
// see: crbug.com/240586
bool IsBlacklisted(const std::string& language_code) {
  return language_code == "si";  // Sinhala
}

// Gets the list of languages with |descriptors| based on |base_language_codes|.
// The |most_relevant_language_codes| will be first in the list. If
// |insert_divider| is true, an entry with its "code" attribute set to
// kMostRelevantLanguagesDivider is placed between the most relevant languages
// and all others.
scoped_ptr<base::ListValue> GetLanguageList(
    const input_method::InputMethodDescriptors& descriptors,
    const std::vector<std::string>& base_language_codes,
    const std::vector<std::string>& most_relevant_language_codes,
    bool insert_divider) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();

  std::set<std::string> language_codes;
  // Collect the language codes from the supported input methods.
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor = descriptors[i];
    const std::vector<std::string>& languages = descriptor.language_codes();
    for (size_t i = 0; i < languages.size(); ++i)
      language_codes.insert(languages[i]);
  }

  // Language sort order.
  std::map<std::string, int /* index */> language_index;
  for (size_t i = 0; i < most_relevant_language_codes.size(); ++i)
    language_index[most_relevant_language_codes[i]] = i;

  // Map of display name -> {language code, native_display_name}.
  // In theory, we should be able to create a map that is sorted by
  // display names using ICU comparator, but doing it is hard, thus we'll
  // use an auxiliary vector to achieve the same result.
  typedef std::pair<std::string, base::string16> LanguagePair;
  typedef std::map<base::string16, LanguagePair> LanguageMap;
  LanguageMap language_map;

  // The auxiliary vector mentioned above (except the most relevant locales).
  std::vector<base::string16> display_names;

  // Separate vector of the most relevant locales.
  std::vector<base::string16> most_relevant_locales_display_names(
      most_relevant_language_codes.size());

  size_t most_relevant_locales_count = 0;

  // Build the list of display names, and build the language map.

  // The list of configured locales might have entries not in
  // base_language_codes. If there are unsupported language variants,
  // but they resolve to backup locale within base_language_codes, also
  // add them to the list.
  for (std::map<std::string, int>::const_iterator it = language_index.begin();
       it != language_index.end(); ++it) {
    const std::string& language_id = it->first;

    const size_t dash_pos = language_id.find_first_of('-');

    // Ignore non-specific codes.
    if (dash_pos == std::string::npos || dash_pos == 0)
      continue;

    if (std::find(base_language_codes.begin(),
                  base_language_codes.end(),
                  language_id) != base_language_codes.end()) {
      // Language is supported. No need to replace
      continue;
    }
    std::string resolved_locale;
    if (!l10n_util::CheckAndResolveLocale(language_id, &resolved_locale))
      continue;

    if (std::find(base_language_codes.begin(),
                  base_language_codes.end(),
                  resolved_locale) == base_language_codes.end()) {
      // Resolved locale is not supported.
      continue;
    }

    const base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(language_id, app_locale, true);
    const base::string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(
            language_id, language_id, true);

    language_map[display_name] =
        std::make_pair(language_id, native_display_name);

    most_relevant_locales_display_names[it->second] = display_name;
    ++most_relevant_locales_count;
  }

  // Translate language codes, generated from input methods.
  for (std::set<std::string>::const_iterator it = language_codes.begin();
       it != language_codes.end(); ++it) {
     // Exclude the language which is not in |base_langauge_codes| even it has
     // input methods.
    if (std::find(base_language_codes.begin(),
                  base_language_codes.end(),
                  *it) == base_language_codes.end()) {
      continue;
    }

    const base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(*it, app_locale, true);
    const base::string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(*it, *it, true);

    language_map[display_name] =
        std::make_pair(*it, native_display_name);

    const std::map<std::string, int>::const_iterator index_pos =
        language_index.find(*it);
    if (index_pos != language_index.end()) {
      base::string16& stored_display_name =
          most_relevant_locales_display_names[index_pos->second];
      if (stored_display_name.empty()) {
        stored_display_name = display_name;
        ++most_relevant_locales_count;
      }
    } else {
      display_names.push_back(display_name);
    }
  }
  DCHECK_EQ(display_names.size() + most_relevant_locales_count,
            language_map.size());

  // Build the list of display names, and build the language map.
  for (size_t i = 0; i < base_language_codes.size(); ++i) {
    // Skip this language if it was already added.
    if (language_codes.find(base_language_codes[i]) != language_codes.end())
      continue;

    // TODO(zork): Remove this blacklist when fonts are added to Chrome OS.
    // see: crbug.com/240586
    if (IsBlacklisted(base_language_codes[i]))
      continue;

    base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(
            base_language_codes[i], app_locale, false);
    base::string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(
            base_language_codes[i], base_language_codes[i], false);
    language_map[display_name] =
        std::make_pair(base_language_codes[i], native_display_name);

    const std::map<std::string, int>::const_iterator index_pos =
        language_index.find(base_language_codes[i]);
    if (index_pos != language_index.end()) {
      most_relevant_locales_display_names[index_pos->second] = display_name;
      ++most_relevant_locales_count;
    } else {
      display_names.push_back(display_name);
    }
  }

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings16(app_locale, &display_names);
  // Concatenate most_relevant_locales_display_names and display_names.
  // Insert special divider in between.
  std::vector<base::string16> out_display_names;
  for (size_t i = 0; i < most_relevant_locales_display_names.size(); ++i) {
    if (most_relevant_locales_display_names[i].size() == 0)
      continue;
    out_display_names.push_back(most_relevant_locales_display_names[i]);
  }

  base::string16 divider16;
  if (insert_divider) {
    divider16 = base::ASCIIToUTF16(kMostRelevantLanguagesDivider);
    out_display_names.push_back(divider16);
  }

  std::copy(display_names.begin(),
            display_names.end(),
            std::back_inserter(out_display_names));

  // Build the language list from the language map.
  scoped_ptr<base::ListValue> language_list(new base::ListValue());
  for (size_t i = 0; i < out_display_names.size(); ++i) {
    // Sets the directionality of the display language name.
    base::string16 display_name(out_display_names[i]);
    if (insert_divider && display_name == divider16) {
      // Insert divider.
      base::DictionaryValue* dictionary = new base::DictionaryValue();
      dictionary->SetString("code", kMostRelevantLanguagesDivider);
      language_list->Append(dictionary);
      continue;
    }
    const bool markup_removal =
        base::i18n::UnadjustStringForLocaleDirection(&display_name);
    DCHECK(markup_removal);
    const bool has_rtl_chars =
        base::i18n::StringContainsStrongRTLChars(display_name);
    const std::string directionality = has_rtl_chars ? "rtl" : "ltr";

    const LanguagePair& pair = language_map[out_display_names[i]];
    base::DictionaryValue* dictionary = new base::DictionaryValue();
    dictionary->SetString("code", pair.first);
    dictionary->SetString("displayName", out_display_names[i]);
    dictionary->SetString("textDirection", directionality);
    dictionary->SetString("nativeDisplayName", pair.second);
    language_list->Append(dictionary);
  }

  return language_list.Pass();
}

}  // namespace

const char kMostRelevantLanguagesDivider[] = "MOST_RELEVANT_LANGUAGES_DIVIDER";

scoped_ptr<base::ListValue> GetUILanguageList(
    const std::vector<std::string>* most_relevant_language_codes,
    const std::string& selected) {
  ComponentExtensionIMEManager* manager =
      input_method::InputMethodManager::Get()->
          GetComponentExtensionIMEManager();
  input_method::InputMethodDescriptors descriptors;
  if (manager->IsInitialized())
    descriptors = manager->GetXkbIMEAsInputMethodDescriptor();
  scoped_ptr<base::ListValue> languages_list(GetLanguageList(
      descriptors,
      l10n_util::GetAvailableLocales(),
      most_relevant_language_codes
          ? *most_relevant_language_codes
          : StartupCustomizationDocument::GetInstance()->configured_locales(),
      true));

  for (size_t i = 0; i < languages_list->GetSize(); ++i) {
    base::DictionaryValue* language_info = NULL;
    if (!languages_list->GetDictionary(i, &language_info))
      NOTREACHED();

    std::string value;
    language_info->GetString("code", &value);
    std::string display_name;
    language_info->GetString("displayName", &display_name);
    std::string native_name;
    language_info->GetString("nativeDisplayName", &native_name);

    // If it's an option group divider, add field name.
    if (value == kMostRelevantLanguagesDivider) {
      language_info->SetString(
          "optionGroupName",
          l10n_util::GetStringUTF16(IDS_OOBE_OTHER_LANGUAGES));
    }
    if (display_name != native_name) {
      display_name = base::StringPrintf("%s - %s",
                                        display_name.c_str(),
                                        native_name.c_str());
    }

    language_info->SetString("value", value);
    language_info->SetString("title", display_name);
    if (value == selected)
      language_info->SetBoolean("selected", true);
  }
  return languages_list.Pass();
}

scoped_ptr<base::ListValue> GetAcceptLanguageList() {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> accept_language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &accept_language_codes);
  return GetLanguageList(
      *input_method::InputMethodManager::Get()->GetSupportedInputMethods(),
      accept_language_codes,
      StartupCustomizationDocument::GetInstance()->configured_locales(),
      false);
}

scoped_ptr<base::ListValue> GetLoginKeyboardLayouts(
    const std::string& locale,
    const std::string& selected) {
  scoped_ptr<base::ListValue> input_methods_list(new base::ListValue);
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  if (!manager->GetComponentExtensionIMEManager()->IsInitialized()) {
    input_method::InputMethodDescriptor fallback =
        util->GetFallbackInputMethodDescriptor();
    input_methods_list->Append(
        CreateInputMethodsEntry(fallback, fallback.id()));
    return input_methods_list.Pass();
  }

  const std::vector<std::string>& hardware_login_input_methods =
      util->GetHardwareLoginInputMethodIds();
  manager->EnableLoginLayouts(locale, hardware_login_input_methods);

  scoped_ptr<input_method::InputMethodDescriptors> input_methods(
      manager->GetActiveInputMethods());
  std::set<std::string> input_methods_added;

  for (std::vector<std::string>::const_iterator i =
           hardware_login_input_methods.begin();
       i != hardware_login_input_methods.end();
       ++i) {
    const input_method::InputMethodDescriptor* ime =
        util->GetInputMethodDescriptorFromId(*i);
    // Do not crash in case of misconfiguration.
    if (ime) {
      input_methods_added.insert(*i);
      input_methods_list->Append(CreateInputMethodsEntry(*ime, selected));
    } else {
      NOTREACHED();
    }
  }

  bool optgroup_added = false;
  for (size_t i = 0; i < input_methods->size(); ++i) {
    // Makes sure the id is in legacy xkb id format.
    const std::string& ime_id = (*input_methods)[i].id();
    if (!InsertString(ime_id, input_methods_added))
      continue;
    if (!optgroup_added) {
      optgroup_added = true;
      AddOptgroupOtherLayouts(input_methods_list.get());
    }
    input_methods_list->Append(CreateInputMethodsEntry((*input_methods)[i],
                                                       selected));
  }

  // "xkb:us::eng" should always be in the list of available layouts.
  const std::string us_keyboard_id =
      util->GetFallbackInputMethodDescriptor().id();
  if (input_methods_added.find(us_keyboard_id) == input_methods_added.end()) {
    const input_method::InputMethodDescriptor* us_eng_descriptor =
        util->GetInputMethodDescriptorFromId(us_keyboard_id);
    DCHECK(us_eng_descriptor);
    if (!optgroup_added) {
      optgroup_added = true;
      AddOptgroupOtherLayouts(input_methods_list.get());
    }
    input_methods_list->Append(CreateInputMethodsEntry(*us_eng_descriptor,
                                                       selected));
  }
  return input_methods_list.Pass();
}

}  // namespace chromeos
