// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_util.h"

namespace extensions {

bool ReadOneOrMoreIntegers(base::Value* value, std::vector<int>* result) {
  if (value->IsType(Value::TYPE_INTEGER)) {
    int v = -1;
    if (!value->GetAsInteger(&v))
      return false;
    result->push_back(v);
    return true;

  } else if (value->IsType(Value::TYPE_LIST)) {
    base::ListValue* values = static_cast<base::ListValue*>(value);
    for (size_t i = 0; i < values->GetSize(); ++i) {
      int v = -1;
      if (!values->GetInteger(i, &v))
        return false;
      result->push_back(v);
    }
    return true;
  }
  return false;
}

} // namespace extensions
