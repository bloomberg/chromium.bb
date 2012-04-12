// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_helpers.h"

#include "base/values.h"

namespace extensions {
namespace declarative_webrequest_helpers {

// Converts a ValueList |value| of strings into a vector. Returns true if
// successful.
bool GetAsStringVector(const base::Value* value,
                       std::vector<std::string>* out) {
  const ListValue* value_as_list = 0;
  if (!value->GetAsList(&value_as_list))
    return false;

  size_t number_types = value_as_list->GetSize();
  for (size_t i = 0; i < number_types; ++i) {
    std::string item;
    if (!value_as_list->GetString(i, &item))
      return false;
    out->push_back(item);
  }
  return true;
}

}  // namespace declarative_webrequest_helpers
}  // namespace extensions
