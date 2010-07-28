// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "language_options_util.h"

#include "app/l10n_util.h"
#include "base/values.h"

namespace chromeos {

ListValue* CreateMultipleChoiceList(
    const LanguageMultipleChoicePreference<const char*>& preference) {
  int list_length = 0;
  for (size_t i = 0;
       i < LanguageMultipleChoicePreference<const char*>::kMaxItems;
       ++i) {
    if (preference.values_and_ids[i].item_message_id == 0)
      break;
    ++list_length;
  }
  DCHECK_GT(list_length, 0);

  ListValue* list_value = new ListValue();
  for (int i = 0; i < list_length; ++i) {
    ListValue* option = new ListValue();
    option->Append(Value::CreateStringValue(
        preference.values_and_ids[i].ibus_config_value));
    option->Append(Value::CreateStringValue(l10n_util::GetString(
        preference.values_and_ids[i].item_message_id)));
    list_value->Append(option);
  }
  return list_value;
}

}  // namespace chromeos
