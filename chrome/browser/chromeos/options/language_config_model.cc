// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_config_model.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "app/l10n_util.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

namespace chromeos {

AddLanguageComboboxModel::AddLanguageComboboxModel(
    Profile* profile,
    const std::vector<std::string>& locale_codes)
    : LanguageComboboxModel(profile, locale_codes) {
}

int AddLanguageComboboxModel::GetItemCount() {
  // +1 for "Add language".
  return get_languages_count() + 1 - ignore_set_.size();
}

string16 AddLanguageComboboxModel::GetItemAt(int index) {
  // Show "Add language" as the first item.
  if (index == 0) {
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_LANGUAGE_COMBOBOX);
  }
  return GetLanguageNameAt(GetLanguageIndex(index));
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
  // Initialize the maps and vectors.
  InitInputMethodIdVectors();

  preferred_languages_pref_.Init(
      prefs::kLanguagePreferredLanguages, pref_service_, this);
  preload_engines_pref_.Init(
      prefs::kLanguagePreloadEngines, pref_service_, this);
  // TODO(yusukes): It might be safer to call GetActiveLanguages() cros API
  // here and compare the result and preload_engines_pref_.GetValue().
  // If there's a discrepancy between IBus setting and Chrome prefs, we
  // can resolve it by calling preload_engines_pref_SetValue() here.

  // Initialize the language codes currently activated.
  NotifyPrefChanged();
}

size_t LanguageConfigModel::CountNumActiveInputMethods(
    const std::string& language_code) {
  int num_selected_active_input_methods = 0;
  std::vector<std::string> input_method_ids;
  input_method::GetInputMethodIdsFromLanguageCode(
      language_code, input_method::kAllInputMethods, &input_method_ids);
  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    if (InputMethodIsActivated(input_method_ids[i])) {
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
  input_method::SortLanguageCodesByNames(&preferred_language_codes_);
  // Find the language code just added in the sorted language codes.
  const int added_at =
      std::distance(preferred_language_codes_.begin(),
                    std::find(preferred_language_codes_.begin(),
                              preferred_language_codes_.end(),
                              language_code));
  preferred_languages_pref_.SetValue(
      JoinString(preferred_language_codes_, ','));
  return added_at;
}

void LanguageConfigModel::RemoveLanguageAt(size_t row) {
  preferred_language_codes_.erase(preferred_language_codes_.begin() + row);
  preferred_languages_pref_.SetValue(
      JoinString(preferred_language_codes_, ','));
}

void LanguageConfigModel::UpdateInputMethodPreferences(
    const std::vector<std::string>& in_new_input_method_ids) {
  std::vector<std::string> new_input_method_ids = in_new_input_method_ids;
  // Note: Since |new_input_method_ids| is alphabetically sorted and the sort
  // function below uses stable sort, the relateve order of input methods that
  // belong to the same language (e.g. "mozc" and "xkb:jp::jpn") is maintained.
  input_method::SortInputMethodIdsByNames(&new_input_method_ids);
  preload_engines_pref_.SetValue(JoinString(new_input_method_ids, ','));
}

void LanguageConfigModel::DeactivateInputMethodsFor(
    const std::string& language_code) {
  for (size_t i = 0; i < num_supported_input_method_ids(); ++i) {
    if (input_method::GetLanguageCodeFromInputMethodId(
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
  const std::string value = preload_engines_pref_.GetValue();
  out_input_method_ids->clear();
  if (!value.empty())
    base::SplitString(value, ',', out_input_method_ids);
}

void LanguageConfigModel::GetPreferredLanguageCodes(
    std::vector<std::string>* out_language_codes) {
  const std::string value = preferred_languages_pref_.GetValue();
  out_language_codes->clear();
  if (!value.empty())
    base::SplitString(value, ',', out_language_codes);
}

void LanguageConfigModel::GetInputMethodIdsFromLanguageCode(
    const std::string& language_code,
    std::vector<std::string>* input_method_ids) const {
  DCHECK(input_method_ids);
  input_method_ids->clear();
  input_method::GetInputMethodIdsFromLanguageCode(
      language_code, input_method::kAllInputMethods, input_method_ids);
}

void LanguageConfigModel::NotifyPrefChanged() {
  GetPreferredLanguageCodes(&preferred_language_codes_);
}

void LanguageConfigModel::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    NotifyPrefChanged();
  }
}

void LanguageConfigModel::InitInputMethodIdVectors() {
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
        input_method::GetLanguageCodeFromDescriptor(input_method);
    // Add the language code and the input method id to the sets.
    supported_language_code_set.insert(language_code);
    supported_input_method_id_set.insert(input_method.id);
    // Remember the pair.
    id_to_descriptor_map.insert(
        std::make_pair(input_method.id, &input_method));
  }

  // Go through the languages listed in kExtraLanguages.
  for (size_t i = 0; i < arraysize(input_method::kExtraLanguages); ++i) {
    const char* language_code = input_method::kExtraLanguages[i].language_code;
    const char* input_method_id =
        input_method::kExtraLanguages[i].input_method_id;
    std::map<std::string, const InputMethodDescriptor*>::const_iterator iter =
        id_to_descriptor_map.find(input_method_id);
    // If the associated input method descriptor is found, add the
    // language code and the input method.
    if (iter != id_to_descriptor_map.end()) {
      const InputMethodDescriptor& input_method = *(iter->second);
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

}  // namespace chromeos
