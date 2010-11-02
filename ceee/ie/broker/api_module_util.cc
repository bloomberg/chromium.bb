// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities used by the API modules.

#include "ceee/ie/broker/api_module_util.h"

namespace api_module_util {

bool GetListFromJsonString(const std::string& input_list_args,
                           scoped_ptr<ListValue>* list) {
  DCHECK(list != NULL);
  scoped_ptr<Value> input_args_val(base::JSONReader::Read(input_list_args,
                                                          true));
  DCHECK(input_args_val != NULL && input_args_val->IsType(Value::TYPE_LIST));
  if (input_args_val == NULL || !input_args_val->IsType(Value::TYPE_LIST))
    return false;
  list->reset(static_cast<ListValue*>(input_args_val.release()));
  return true;
}

Value* GetListAndFirstValue(const std::string& input_list_args,
                            scoped_ptr<ListValue>* list) {
  if (!GetListFromJsonString(input_list_args, list))
    return NULL;
  Value* value = NULL;
  if (!(*list)->Get(0, &value) || value == NULL) {
    DCHECK(false) << "Input arguments are not a non-empty list.";
  }
  return value;
}

bool GetListAndIntegerValue(const std::string& input_list_args,
                            scoped_ptr<ListValue>* list,
                            int* int_value) {
  Value* value = GetListAndFirstValue(input_list_args, list);
  DCHECK(value != NULL && value->IsType(Value::TYPE_INTEGER));
  if (value == NULL || !value->IsType(Value::TYPE_INTEGER)) {
    return false;
  }
  return value->GetAsInteger(int_value);
}

DictionaryValue* GetListAndDictionaryValue(const std::string& input_list_args,
                                           scoped_ptr<ListValue>* list) {
  Value* value = GetListAndFirstValue(input_list_args, list);
  DCHECK(value != NULL && value->IsType(Value::TYPE_DICTIONARY));
  if (value == NULL || !value->IsType(Value::TYPE_DICTIONARY)) {
    return NULL;
  }
  return static_cast<DictionaryValue*>(value);
}

bool GetListAndDictIntValue(const std::string& input_list_args,
                            const char* dict_value_key_name,
                            scoped_ptr<ListValue>* list,
                            int* int_value) {
  DictionaryValue* dict = GetListAndDictionaryValue(input_list_args, list);
  if (dict == NULL)
    return false;
  return dict->GetInteger(dict_value_key_name, int_value);
}

}  // namespace api_module_util
