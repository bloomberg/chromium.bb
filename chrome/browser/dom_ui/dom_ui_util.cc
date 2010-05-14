// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_util.h"

#include "base/logging.h"
#include "base/values.h"

namespace dom_ui_util {

std::string GetJsonResponseFromFirstArgumentInList(const Value* content) {
  return GetJsonResponseFromArgumentList(content, 0);
}

std::string GetJsonResponseFromArgumentList(const Value* content,
                                            size_t list_index) {
  std::string result;

  if (!content || !content->IsType(Value::TYPE_LIST))  {
    NOTREACHED();
    return result;
  }
  const ListValue* args = static_cast<const ListValue*>(content);
  if (args->GetSize() <= list_index) {
    NOTREACHED();
    return result;
  }

  Value* value = NULL;
  if (args->Get(list_index, &value))
    value->GetAsString(&result);

  return result;
}

}  // end of namespace dom_ui_util
