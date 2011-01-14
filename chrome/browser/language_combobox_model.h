// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_LANGUAGE_COMBOBOX_MODEL_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/base/models/combobox_model.h"

class Profile;

///////////////////////////////////////////////////////////////////////////////
// LanguageList
//  Provides code to enumerate locale names for language selection lists.
//  To be used by combobox, menu or other models.
class LanguageList {
 public:
  struct LocaleData {
    LocaleData() { }
    LocaleData(const string16& name, const std::string& code)
        : native_name(name), locale_code(code) { }

    string16 native_name;
    std::string locale_code;  // E.g., en-us.
  };
  typedef std::map<string16, LocaleData> LocaleDataMap;

  LanguageList();

  explicit LanguageList(const std::vector<std::string>& locale_codes);

  virtual ~LanguageList();

  // Duplicates specified languages at the beginning of the list for
  // easier access.
  void CopySpecifiedLanguagesUp(const std::string& locale_codes);

  int get_languages_count() const;

  string16 GetLanguageNameAt(int index) const;

  // Return the locale for the given index.  E.g., may return pt-BR.
  std::string GetLocaleFromIndex(int index) const;

  // Returns the index for the given locale.  Returns -1 if the locale is not
  // in the combobox model.
  int GetIndexFromLocale(const std::string& locale) const;

 private:
  // The names of all the locales in the current application locale.
  std::vector<string16> locale_names_;

  // A map of some extra data (LocaleData) keyed off the name of the locale.
  LocaleDataMap native_names_;

  void InitNativeNames(const std::vector<std::string>& locale_codes);

  DISALLOW_COPY_AND_ASSIGN(LanguageList);
};

///////////////////////////////////////////////////////////////////////////////
// LanguageComboboxModel
//  The combobox model implementation.
class LanguageComboboxModel : public LanguageList, public ui::ComboboxModel {
 public:
  LanguageComboboxModel();

  // Temporary compatibility constructor.
  LanguageComboboxModel(Profile* profile,
                        const std::vector<std::string>& locale_codes);

  virtual ~LanguageComboboxModel();

  virtual int GetItemCount();
  virtual string16 GetItemAt(int index);

  // Returns the index of the language currently specified in the user's
  // preference file.  Note that it's possible for language A to be picked
  // while chrome is currently in language B if the user specified language B
  // via --lang.  Since --lang is not a persistent setting, it seems that it
  // shouldn't be reflected in this combo box.  We return -1 if the value in
  // the pref doesn't map to a know language (possible if the user edited the
  // prefs file manually).
  int GetSelectedLanguageIndex(const std::string& prefs);

 private:
  // Profile.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LanguageComboboxModel);
};

#endif  // CHROME_BROWSER_LANGUAGE_COMBOBOX_MODEL_H_
