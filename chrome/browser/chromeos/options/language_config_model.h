// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_MODEL_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/language_combobox_model.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "cros/chromeos_input_method.h"

namespace chromeos {

// The combobox model is used for adding languages in the language config
// view.
class AddLanguageComboboxModel : public ::LanguageComboboxModel {
 public:
  AddLanguageComboboxModel(Profile* profile,
                           const std::vector<std::string>& locale_codes);
  // LanguageComboboxModel overrides.
  virtual int GetItemCount();
  virtual string16 GetItemAt(int index);

  // Converts the given index (index of the items in the combobox) to the
  // index of the internal language list. The returned index can be used
  // for GetLocaleFromIndex() and GetLanguageNameAt().
  int GetLanguageIndex(int index) const;

  // Marks the given language code to be ignored. Ignored languages won't
  // be shown in the combobox. It would be simpler if we could remove and
  // add language codes from the model, but ComboboxModel does not allow
  // items to be added/removed. Thus we use |ignore_set_| instead.
  void SetIgnored(const std::string& language_code, bool ignored);

 private:
  std::set<std::string> ignore_set_;
  DISALLOW_COPY_AND_ASSIGN(AddLanguageComboboxModel);
};

// The model of LanguageConfigView.
class LanguageConfigModel : public NotificationObserver {
 public:
  explicit LanguageConfigModel(PrefService* pref_service);

  // Counts the number of active input methods for the given language code.
  size_t CountNumActiveInputMethods(const std::string& language_code);

  // Returns true if the language code is in the preferred language list.
  bool HasLanguageCode(const std::string& language_code) const;

  // Adds the given language to the preferred language list, and returns
  // the index of the row where the language is added.
  size_t AddLanguageCode(const std::string& language_code);

  // Removes the language at the given row.
  void RemoveLanguageAt(size_t row);

  // Updates Chrome's input method preferences.
  void UpdateInputMethodPreferences(
      const std::vector<std::string>& new_input_method_ids);

  // Deactivates the input methods for the given language code.
  void DeactivateInputMethodsFor(const std::string& language_code);

  // Activates or deactivates an IME whose ID is |input_method_id|.
  void SetInputMethodActivated(const std::string& input_method_id,
                               bool activated);

  // Returns true if an IME of |input_method_id| is activated.
  bool InputMethodIsActivated(const std::string& input_method_id);

  // Gets the list of active IME IDs like "pinyin" and "m17n:ar:kbd" from
  // the underlying preference object. The original contents of
  // |out_input_method_ids| are lost.
  void GetActiveInputMethodIds(
      std::vector<std::string>* out_input_method_ids);

  // Gets the list of preferred language codes like "en-US" and "fr" from
  // the underlying preference object. The original contents of
  // |out_language_codes| are lost.
  void GetPreferredLanguageCodes(
      std::vector<std::string>* out_language_codes);

  // Gets the list of input method ids associated with the given language
  // code.  The original contents of |input_method_ids| will be lost.
  void GetInputMethodIdsFromLanguageCode(
      const std::string& language_code,
      std::vector<std::string>* input_method_ids) const;

  // Callback for |preferred_language_codes_| pref updates. Initializes
  // the preferred language codes based on the updated pref value.
  void NotifyPrefChanged();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  const std::string& preferred_language_code_at(size_t at) const {
    return preferred_language_codes_[at];
  }

  size_t num_preferred_language_codes() const {
    return preferred_language_codes_.size();
  }

  const std::string& supported_input_method_id_at(size_t at) const {
    return supported_input_method_ids_[at];
  }

  size_t num_supported_input_method_ids() const {
    return supported_input_method_ids_.size();
  }

  const std::vector<std::string>& supported_language_codes() const {
    return supported_language_codes_;
  }

 private:
  // Initializes id_to_{code,display_name}_map_ maps,
  // as well as supported_{language_codes,input_method_ids}_ vectors.
  void InitInputMethodIdVectors();

  PrefService* pref_service_;
  // The codes of the preferred languages.
  std::vector<std::string> preferred_language_codes_;
  StringPrefMember preferred_languages_pref_;
  StringPrefMember preload_engines_pref_;
  // List of supported language codes like "en" and "ja".
  std::vector<std::string> supported_language_codes_;
  // List of supported IME IDs like "pinyin" and "m17n:ar:kbd".
  std::vector<std::string> supported_input_method_ids_;

  DISALLOW_COPY_AND_ASSIGN(LanguageConfigModel);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_MODEL_H_
