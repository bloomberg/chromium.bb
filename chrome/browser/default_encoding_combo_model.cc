// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_encoding_combo_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

DefaultEncodingComboboxModel::DefaultEncodingComboboxModel() {
  // Initialize the vector of all sorted encodings according to current
  // UI locale.
  std::string locale = g_browser_process->GetApplicationLocale();
  int size = CharacterEncoding::GetSupportCanonicalEncodingCount();
  for (int i = 0; i < size; ++i) {
    sorted_encoding_list_.push_back(CharacterEncoding::EncodingInfo(
        CharacterEncoding::GetEncodingCommandIdByIndex(i)));
  }
  l10n_util::SortVectorWithStringKey(locale, &sorted_encoding_list_, true);
}

DefaultEncodingComboboxModel::~DefaultEncodingComboboxModel() {
}

int DefaultEncodingComboboxModel::GetItemCount() {
  return static_cast<int>(sorted_encoding_list_.size());
}

string16 DefaultEncodingComboboxModel::GetItemAt(int index) {
  DCHECK(index >= 0 && index < GetItemCount());
  return sorted_encoding_list_[index].encoding_display_name;
}

std::string DefaultEncodingComboboxModel::GetEncodingCharsetByIndex(int index) {
  DCHECK(index >= 0 && index < GetItemCount());
  int encoding_id = sorted_encoding_list_[index].encoding_id;
  return CharacterEncoding::GetCanonicalEncodingNameByCommandId(encoding_id);
}

int DefaultEncodingComboboxModel::GetSelectedEncodingIndex(Profile* profile) {
  StringPrefMember current_encoding_string;
  current_encoding_string.Init(prefs::kDefaultCharset,
                               profile->GetPrefs(),
                               NULL);
  const std::string current_encoding = current_encoding_string.GetValue();
  for (int i = 0; i < GetItemCount(); ++i) {
    if (GetEncodingCharsetByIndex(i) == current_encoding)
      return i;
  }

  return 0;
}
