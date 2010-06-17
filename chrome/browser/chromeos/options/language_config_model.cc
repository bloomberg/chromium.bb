// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_config_model.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "app/l10n_util_collator.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/status/language_menu_l10n_util.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

namespace chromeos {

namespace {

// The code should be compatible with one of codes used for UI languages,
// defined in app/l10_util.cc.
const char kDefaultLanguageCode[] = "en-US";

// The list of language that do not have associated input methods. For
// these languages, we associate input methods here.
const struct ExtraLanguage {
  const char* language_code;
  const char* input_method_id;
} kExtraLanguages[] = {
  { "id", "xkb:us::eng" }, // For Indonesian, use US keyboard layout.
  // The code "fil" comes from app/l10_util.cc.
  { "fil", "xkb:us::eng" },  // For Filipino, use US keyboard layout.
  // The code "es-419" comes from app/l10_util.cc.
  // For Spanish in Latin America, use Spanish keyboard layout.
  { "es-419", "xkb:es::spa" },
};

// The list defines pairs of language code and the default input method
// id. The list is used for reordering input method ids.
//
// TODO(satorux): We may need to handle secondary, and ternary input
// methods, rather than handling the default input method only.
const struct LanguageDefaultInputMethodId {
  const char* language_code;
  const char* input_method_id;
} kLanguageDefaultInputMethodIds[] = {
  { "en-US", "xkb:us::eng", },  // US - English
  { "fr",    "xkb:fr::fra", },  // France - French
  { "de",    "xkb:de::ger", },  // Germany - German
};

}  // namespace

AddLanguageComboboxModel::AddLanguageComboboxModel(
    Profile* profile,
    const std::vector<std::string>& locale_codes)
    : LanguageComboboxModel(profile, locale_codes) {
}

int AddLanguageComboboxModel::GetItemCount() {
  // +1 for "Add language".
  return get_languages_count() + 1 - ignore_set_.size();
}

std::wstring AddLanguageComboboxModel::GetItemAt(int index) {
  // Show "Add language" as the first item.
  if (index == 0) {
    return l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_LANGUAGE_COMBOBOX);
  }
  return LanguageConfigModel::MaybeRewriteLanguageName(
      GetLanguageNameAt(GetLanguageIndex(index)));
}

int AddLanguageComboboxModel::GetLanguageIndex(int index) const {
  // The adjusted_index is counted while ignoring languages in ignore_set_.
  int adjusted_index = 0;
  for (int i = 0; i < get_languages_count(); ++i) {
    if (ignore_set_.count(GetLocaleFromIndex(i)) > 0) {
      continue;
    }
    // -1 for "Add language".
    if (adjusted_index == index - 1) {
      return i;
    }
    ++adjusted_index;
  }
  return 0;
}

void AddLanguageComboboxModel::SetIgnored(
    const std::string& language_code, bool ignored) {
  if (ignored) {
    // Add to the ignore_set_ if the language code is known (i.e. reject
    // unknown language codes just in case).
    if (GetIndexFromLocale(language_code) != -1) {
      ignore_set_.insert(language_code);
    } else {
      LOG(ERROR) << "Unknown language code: " << language_code;
    }
  } else {
    ignore_set_.erase(language_code);
  }
}

LanguageConfigModel::LanguageConfigModel(PrefService* pref_service)
    : pref_service_(pref_service) {
}

void LanguageConfigModel::Init() {
  // Initialize the maps and vectors.
  InitInputMethodIdMapsAndVectors();

  preload_engines_.Init(
      prefs::kLanguagePreloadEngines, pref_service_, this);
  // TODO(yusukes): It might be safer to call GetActiveLanguages() cros API
  // here and compare the result and preload_engines_.GetValue(). If there's
  // a discrepancy between IBus setting and Chrome prefs, we can resolve it
  // by calling preload_engines_SetValue() here.
}

size_t LanguageConfigModel::CountNumActiveInputMethods(
    const std::string& language_code) {
  int num_selected_active_input_methods = 0;
  std::pair<LanguageCodeToIdsMap::const_iterator,
            LanguageCodeToIdsMap::const_iterator> range =
      language_code_to_ids_map_.equal_range(language_code);
  for (LanguageCodeToIdsMap::const_iterator iter = range.first;
       iter != range.second; ++iter) {
    if (InputMethodIsActivated(iter->second)) {
      ++num_selected_active_input_methods;
    }
  }
  return num_selected_active_input_methods;
}

bool LanguageConfigModel::HasLanguageCode(
    const std::string& language_code) const {
  return std::find(preferred_language_codes_.begin(),
                   preferred_language_codes_.end(),
                   language_code) != preferred_language_codes_.end();
}

size_t LanguageConfigModel::AddLanguageCode(
    const std::string& language_code) {
  preferred_language_codes_.push_back(language_code);
  // Sort the language codes by names. This is not efficient, but
  // acceptable as the language list is about 40 item long at most.  In
  // theory, we could find the position to insert rather than sorting, but
  // it would be complex as we need to use unicode string comparator.
  SortLanguageCodesByNames(&preferred_language_codes_);
  // Find the language code just added in the sorted language codes.
  const int added_at =
      std::distance(preferred_language_codes_.begin(),
                    std::find(preferred_language_codes_.begin(),
                              preferred_language_codes_.end(),
                              language_code));
  return added_at;
}

void LanguageConfigModel::RemoveLanguageAt(size_t row) {
  preferred_language_codes_.erase(preferred_language_codes_.begin() + row);
}

void LanguageConfigModel::UpdateInputMethodPreferences(
    const std::vector<std::string>& in_new_input_method_ids) {
  std::vector<std::string> new_input_method_ids = in_new_input_method_ids;
  // Note: Since |new_input_method_ids| is alphabetically sorted and the sort
  // function below uses stable sort, the relateve order of input methods that
  // belong to the same language (e.g. "mozc" and "xkb:jp::jpn") is maintained.
  SortInputMethodIdsByNames(id_to_language_code_map_, &new_input_method_ids);
  preload_engines_.SetValue(UTF8ToWide(JoinString(new_input_method_ids, ',')));
}

void LanguageConfigModel::DeactivateInputMethodsFor(
    const std::string& language_code) {
  for (size_t i = 0; i < num_supported_input_method_ids(); ++i) {
    if (GetLanguageCodeFromInputMethodId(
            supported_input_method_id_at(i)) ==
        language_code) {
      // What happens if we disable the input method currently active?
      // IBus should take care of it, so we don't do anything special
      // here. See crosbug.com/2443.
      SetInputMethodActivated(supported_input_method_id_at(i), false);
      // Do not break; here in order to disable all engines that belong to
      // |language_code|.
    }
  }
}

void LanguageConfigModel::SetInputMethodActivated(
    const std::string& input_method_id, bool activated) {
  DCHECK(!input_method_id.empty());
  std::vector<std::string> input_method_ids;
  GetActiveInputMethodIds(&input_method_ids);

  std::set<std::string> input_method_id_set(input_method_ids.begin(),
                                            input_method_ids.end());
  if (activated) {
    // Add |id| if it's not already added.
    input_method_id_set.insert(input_method_id);
  } else {
    input_method_id_set.erase(input_method_id);
  }

  // Update Chrome's preference.
  std::vector<std::string> new_input_method_ids(input_method_id_set.begin(),
                                                input_method_id_set.end());
  UpdateInputMethodPreferences(new_input_method_ids);
}

bool LanguageConfigModel::InputMethodIsActivated(
    const std::string& input_method_id) {
  std::vector<std::string> input_method_ids;
  GetActiveInputMethodIds(&input_method_ids);
  return (std::find(input_method_ids.begin(), input_method_ids.end(),
                    input_method_id) != input_method_ids.end());
}

void LanguageConfigModel::GetActiveInputMethodIds(
    std::vector<std::string>* out_input_method_ids) {
  const std::wstring value = preload_engines_.GetValue();
  out_input_method_ids->clear();
  if (!value.empty()) {
    SplitString(WideToUTF8(value), ',', out_input_method_ids);
  }
}

std::string LanguageConfigModel::GetLanguageCodeFromInputMethodId(
    const std::string& input_method_id) const {
  std::map<std::string, std::string>::const_iterator iter
      = id_to_language_code_map_.find(input_method_id);
  return (iter == id_to_language_code_map_.end()) ?
      // Returning |kDefaultLanguageCode| is not for Chrome OS but for Ubuntu
      // where the ibus-xkb-layouts module could be missing.
      kDefaultLanguageCode : iter->second;
}

std::string LanguageConfigModel::GetInputMethodDisplayNameFromId(
    const std::string& input_method_id) const {
  // |kDefaultDisplayName| is not for Chrome OS. See the comment above.
  static const char kDefaultDisplayName[] = "USA";
  std::map<std::string, std::string>::const_iterator iter
      = id_to_display_name_map_.find(input_method_id);
  return (iter == id_to_display_name_map_.end()) ?
      kDefaultDisplayName : iter->second;
}

void LanguageConfigModel::GetInputMethodIdsFromLanguageCode(
    const std::string& language_code,
    std::vector<std::string>* input_method_ids) const {
  DCHECK(input_method_ids);
  input_method_ids->clear();

  std::pair<LanguageCodeToIdsMap::const_iterator,
            LanguageCodeToIdsMap::const_iterator> range =
      language_code_to_ids_map_.equal_range(language_code);
  for (LanguageCodeToIdsMap::const_iterator iter = range.first;
       iter != range.second; ++iter) {
    input_method_ids->push_back(iter->second);
  }
  // Reorder the input methods.
  ReorderInputMethodIdsForLanguageCode(language_code, input_method_ids);
}

void LanguageConfigModel::NotifyPrefChanged() {
  std::vector<std::string> input_method_ids;
  GetActiveInputMethodIds(&input_method_ids);

  std::set<std::string> language_code_set;
  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    const std::string language_code =
        GetLanguageCodeFromInputMethodId(input_method_ids[i]);
    language_code_set.insert(language_code);
  }

  preferred_language_codes_.clear();
  preferred_language_codes_.assign(
      language_code_set.begin(), language_code_set.end());
  LanguageConfigModel::SortLanguageCodesByNames(&preferred_language_codes_);
}

void LanguageConfigModel::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    NotifyPrefChanged();
  }
}

std::wstring LanguageConfigModel::MaybeRewriteLanguageName(
    const std::wstring& language_name) {
  // "t" is used as the language code for input methods that don't fall
  // under any other languages.
  if (language_name == L"t") {
    return l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS);
  }
  return language_name;
}

std::wstring LanguageConfigModel::GetLanguageDisplayNameFromCode(
    const std::string& language_code) {
  return MaybeRewriteLanguageName(UTF16ToWide(
      l10n_util::GetDisplayNameForLocale(
          language_code, g_browser_process->GetApplicationLocale(),
          true)));
}

namespace {

// The comparator is used for sorting language codes by their
// corresponding language names, using the ICU collator.
struct CompareLanguageCodesByLanguageName
    : std::binary_function<const std::string&, const std::string&, bool> {
  explicit CompareLanguageCodesByLanguageName(icu::Collator* collator)
      : collator_(collator) {
  }

  // Calling GetLanguageDisplayNameFromCode() in the comparator is not
  // efficient, but acceptable as the function is cheap, and the language
  // list is short (about 40 at most).
  bool operator()(const std::string& s1, const std::string& s2) const {
    const std::wstring key1 =
            LanguageConfigModel::GetLanguageDisplayNameFromCode(s1);
    const std::wstring key2 =
            LanguageConfigModel::GetLanguageDisplayNameFromCode(s2);
    return l10n_util::StringComparator<std::wstring>(collator_)(key1, key2);
  }

  icu::Collator* collator_;
};

// The comparator is used for sorting input method ids by their
// corresponding language names, using the ICU collator.
struct CompareInputMethodIdsByLanguageName
    : std::binary_function<const std::string&, const std::string&, bool> {
  CompareInputMethodIdsByLanguageName(
      icu::Collator* collator,
      const std::map<std::string, std::string>& id_to_language_code_map)
      : comparator_(collator),
        id_to_language_code_map_(id_to_language_code_map) {
  }

  bool operator()(const std::string& s1, const std::string& s2) const {
    std::string language_code_1;
    std::map<std::string, std::string>::const_iterator iter =
        id_to_language_code_map_.find(s1);
    if (iter != id_to_language_code_map_.end()) {
      language_code_1 = iter->second;
    }
    std::string language_code_2;
    iter = id_to_language_code_map_.find(s2);
    if (iter != id_to_language_code_map_.end()) {
      language_code_2 = iter->second;
    }
    return comparator_(language_code_1, language_code_2);
  }

  const CompareLanguageCodesByLanguageName comparator_;
  const std::map<std::string, std::string>& id_to_language_code_map_;
};

}  // namespace

void LanguageConfigModel::SortLanguageCodesByNames(
    std::vector<std::string>* language_codes) {
  // We should build collator outside of the comparator. We cannot have
  // scoped_ptr<> in the comparator for a subtle STL reason.
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale locale(g_browser_process->GetApplicationLocale().c_str());
  scoped_ptr<icu::Collator> collator(
      icu::Collator::createInstance(locale, error));
  if (U_FAILURE(error)) {
    collator.reset();
  }
  std::sort(language_codes->begin(), language_codes->end(),
            CompareLanguageCodesByLanguageName(collator.get()));
}

void LanguageConfigModel::SortInputMethodIdsByNames(
    const std::map<std::string, std::string>& id_to_language_code_map,
    std::vector<std::string>* input_method_ids) {
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale locale(g_browser_process->GetApplicationLocale().c_str());
  scoped_ptr<icu::Collator> collator(
      icu::Collator::createInstance(locale, error));
  if (U_FAILURE(error)) {
    collator.reset();
  }
  std::stable_sort(input_method_ids->begin(), input_method_ids->end(),
                   CompareInputMethodIdsByLanguageName(
                       collator.get(), id_to_language_code_map));
}

void LanguageConfigModel::ReorderInputMethodIdsForLanguageCode(
    const std::string& language_code,
    std::vector<std::string>* input_method_ids) {
  for (size_t i = 0; i < arraysize(kLanguageDefaultInputMethodIds); ++i) {
    if (language_code == kLanguageDefaultInputMethodIds[i].language_code) {
      std::vector<std::string>::iterator iter =
          std::find(input_method_ids->begin(), input_method_ids->end(),
                    kLanguageDefaultInputMethodIds[i].input_method_id);
      // If it's not on the top of |input_method_id|, swap it with the top one.
      if (iter != input_method_ids->end() &&
          iter != input_method_ids->begin()) {
        std::swap(*input_method_ids->begin(), *iter);
      }
      break;  // Don't have to check other language codes.
    }
  }
}

void LanguageConfigModel::InitInputMethodIdMapsAndVectors() {
  // The two sets are used to build lists without duplication.
  std::set<std::string> supported_language_code_set;
  std::set<std::string> supported_input_method_id_set;
  // Build the id to descriptor map for handling kExtraLanguages later.
  std::map<std::string, const InputMethodDescriptor*> id_to_descriptor_map;

  // GetSupportedLanguages() never return NULL.
  scoped_ptr<InputMethodDescriptors> supported_input_methods(
      CrosLibrary::Get()->GetInputMethodLibrary()->GetSupportedInputMethods());
  for (size_t i = 0; i < supported_input_methods->size(); ++i) {
    const InputMethodDescriptor& input_method = supported_input_methods->at(i);
    const std::string language_code =
        InputMethodLibrary::GetLanguageCodeFromDescriptor(input_method);
    AddInputMethodToMaps(language_code, input_method);
    // Add the language code and the input method id to the sets.
    supported_language_code_set.insert(language_code);
    supported_input_method_id_set.insert(input_method.id);
    // Remember the pair.
    id_to_descriptor_map.insert(
        std::make_pair(input_method.id, &input_method));
  }

  // Go through the languages listed in kExtraLanguages.
  for (size_t i = 0; i < arraysize(kExtraLanguages); ++i) {
    const char* language_code = kExtraLanguages[i].language_code;
    const char* input_method_id = kExtraLanguages[i].input_method_id;
    std::map<std::string, const InputMethodDescriptor*>::const_iterator iter =
        id_to_descriptor_map.find(input_method_id);
    // If the associated input method descriptor is found, add the
    // language code and the input method.
    if (iter != id_to_descriptor_map.end()) {
      const InputMethodDescriptor& input_method = *(iter->second);
      AddInputMethodToMaps(language_code, input_method);
      // Add the language code and the input method id to the sets.
      supported_language_code_set.insert(language_code);
      supported_input_method_id_set.insert(input_method.id);
    }
  }

  // Build the vectors from the sets.
  supported_language_codes_.assign(supported_language_code_set.begin(),
                                   supported_language_code_set.end());
  supported_input_method_ids_.assign(supported_input_method_id_set.begin(),
                                     supported_input_method_id_set.end());
}

void LanguageConfigModel::AddInputMethodToMaps(
    const std::string& language_code,
    const InputMethodDescriptor& input_method) {
  id_to_language_code_map_.insert(
      std::make_pair(input_method.id, language_code));
  id_to_display_name_map_.insert(
      std::make_pair(input_method.id, LanguageMenuL10nUtil::GetStringUTF8(
          input_method.display_name)));
  language_code_to_ids_map_.insert(
      std::make_pair(language_code, input_method.id));
}

}  // namespace chromeos
