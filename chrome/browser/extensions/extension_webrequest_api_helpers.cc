// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_api_helpers.h"

#include "base/values.h"

namespace extension_webrequest_api_helpers {

// Converts a string to a List of Integers, each in 0..255. Ownership
// of the created list is passed to the caller.
ListValue* StringToCharList(const std::string& s) {
  ListValue* result = new ListValue;
  for (size_t i = 0, n = s.size(); i < n; ++i) {
    result->Append(
        Value::CreateIntegerValue(
            *reinterpret_cast<const unsigned char*>(&s[i])));
  }
  return result;
}

// Converts a list of integer values between 0 and 255 into a string |*out|.
// Returns true if the conversion was successful.
bool CharListToString(ListValue* list, std::string* out) {
  if (!list)
    return false;
  const size_t list_length = list->GetSize();
  out->resize(list_length);
  int value = 0;
  for (size_t i = 0; i < list_length; ++i) {
    if (!list->GetInteger(i, &value) || value < 0 || value > 255)
      return false;
    unsigned char tmp = static_cast<unsigned char>(value);
    (*out)[i] = *reinterpret_cast<char*>(&tmp);
  }
  return true;
}

}  // namespace extension_webrequest_api_helpers
