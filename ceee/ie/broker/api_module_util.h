// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities used by the API modules.

#ifndef CEEE_IE_BROKER_API_MODULE_UTIL_H_
#define CEEE_IE_BROKER_API_MODULE_UTIL_H_

#include <string>

#include "toolband.h"  //NOLINT

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "ceee/ie/broker/api_dispatcher.h"

namespace api_module_util {

// Helper function to fetch a list from a JSON string.
bool GetListFromJsonString(const std::string& input_list_args,
                           scoped_ptr<ListValue>* list);

// Helper function to fetch a list from a JSON string and return its
// first Value. Note that the Value allocation is owned by the list.
// Returns NULL on failures.
Value* GetListAndFirstValue(const std::string& input_list_args,
                            scoped_ptr<ListValue>* list);

// Helper function to fetch a list from a JSON string and return its
// first Value as an integer. Returns false on failures.
bool GetListAndIntegerValue(const std::string& input_list_args,
                            scoped_ptr<ListValue>* list,
                            int* int_value);

// Helper function to fetch a list from a JSON string and return its
// first Value as a DictionaryValue. Note that the DictionaryValue
// allocation is owned by the list. Returns NULL on failures.
DictionaryValue* GetListAndDictionaryValue(const std::string& input_list_args,
                                           scoped_ptr<ListValue>* list);

// Helper function to fetch a list from a JSON string, get its
// first Value as a DictionaryValue and extracts an int from the dict using
// the provided key name. Returns false on failures.
bool GetListAndDictIntValue(const std::string& input_list_args,
                            const char* dict_value_key_name,
                            scoped_ptr<ListValue>* list,
                            int* int_value);

// A function that can be used as a permanent event handler, which converts the
// tab window handle in the input arguments into the corresponding tab ID.
// @tparam tab_id_key_name The key name for the tab ID.
// @param input_args A list of arguments in the form of a JSON string. The first
//        argument is a dictionary. It contains the key tab_id_key_name, whose
//        corresponding value is the tab window handle.
// @param converted_args On success returns a JSON string, in which the tab
//        window handle has been replaced with the actual tab ID; otherwise
//        returns input_args.
// @param dispatcher The dispatcher used to query tab IDs using tab window
//        handles.
template<const char* tab_id_key_name>
bool ConvertTabIdInDictionary(const std::string& input_args,
                              std::string* converted_args,
                              ApiDispatcher* dispatcher) {
  if (converted_args == NULL || dispatcher == NULL) {
    NOTREACHED();
    return false;
  }
  // Fail safe...
  *converted_args = input_args;

  scoped_ptr<ListValue> input_list;
  DictionaryValue* input_dict = GetListAndDictionaryValue(input_args,
                                                          &input_list);
  if (input_dict == NULL) {
    LOG(ERROR)
        << "Failed to get the details object from the list of arguments.";
    return false;
  }

  int tab_handle = -1;
  bool success = input_dict->GetInteger(tab_id_key_name, &tab_handle);
  if (!success) {
    LOG(ERROR) << "Failed to get " << tab_id_key_name << " property.";
    return false;
  }

  HWND tab_window = reinterpret_cast<HWND>(tab_handle);
  int tab_id = kInvalidChromeSessionId;
  if (tab_window != INVALID_HANDLE_VALUE) {
    tab_id = dispatcher->GetTabIdFromHandle(tab_window);
  }
  input_dict->SetInteger(tab_id_key_name, tab_id);

  base::JSONWriter::Write(input_list.get(), false, converted_args);
  return true;
}

}  // namespace api_module_util

#endif  // CEEE_IE_BROKER_API_MODULE_UTIL_H_
