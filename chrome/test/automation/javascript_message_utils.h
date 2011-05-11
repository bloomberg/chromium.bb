// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_JAVASCRIPT_MESSAGE_UTILS_H_
#define CHROME_TEST_AUTOMATION_JAVASCRIPT_MESSAGE_UTILS_H_
#pragma once

#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/test/automation/dom_element_proxy.h"

// ValueConversionTraits contains functions for creating a value from a
// type, and setting a type from a value. This is general-purpose and can
// be moved to a common location if needed.
template <class T>
struct ValueConversionTraits {
};

template <>
struct ValueConversionTraits<int> {
  static Value* CreateValue(int t) {
    return Value::CreateIntegerValue(t);
  }
  static bool SetFromValue(Value* value, int* t) {
    return value->GetAsInteger(t);
  }
};

template <>
struct ValueConversionTraits<bool> {
  static Value* CreateValue(bool t) {
    return Value::CreateBooleanValue(t);
  }
  static bool SetFromValue(Value* value, bool* t) {
    return value->GetAsBoolean(t);
  }
};

template <>
struct ValueConversionTraits<std::string> {
  static Value* CreateValue(const std::string& t) {
    return Value::CreateStringValue(t);
  }
  static bool SetFromValue(Value* value, std::string* t) {
    return value->GetAsString(t);
  }
};

template <>
struct ValueConversionTraits<DOMElementProxy::By> {
  typedef DOMElementProxy::By type;
  static Value* CreateValue(const type& t) {
    DictionaryValue* value = new DictionaryValue();
    std::string by_type;
    switch (t.type()) {
      case type::TYPE_XPATH:
        by_type = "xpath";
        break;
      case type::TYPE_SELECTORS:
        by_type = "selectors";
        break;
      case type::TYPE_TEXT:
        by_type = "text";
        break;
      default:
        NOTREACHED();
        break;
    }
    value->SetString("type", by_type);
    value->SetString("queryString", t.query());
    return value;
  }
};

template <typename T>
struct ValueConversionTraits<std::vector<T> > {
  static Value* CreateValue(const std::vector<T>& t) {
    ListValue* value = new ListValue();
    for (size_t i = 0; i < t.size(); i++) {
      value->Append(ValueConversionTraits<T>::CreateValue(t[i]));
    }
    return value;
  }
  static bool SetFromValue(Value* value, std::vector<T>* t) {
    if (!value->IsType(Value::TYPE_LIST))
      return false;

    ListValue* list_value = static_cast<ListValue*>(value);
    ListValue::const_iterator iter;
    for (iter = list_value->begin(); iter != list_value->end(); ++iter) {
      T inner_value;
      ValueConversionTraits<T>::SetFromValue(*iter, &inner_value);
      t->push_back(inner_value);
    }
    return true;
  }
};

namespace javascript_utils {

// Converts |arg| to a JSON string.
template <typename T>
std::string JSONStringify(const T& arg) {
  std::string javascript;
  scoped_ptr<Value> value(ValueConversionTraits<T>::CreateValue(arg));
  base::JSONWriter::Write(value.get(), false, &javascript);
  return javascript;
}

// Converts |arg| to a JSON string and returns a string formatted as
// |format| specifies. |format| should only expect string arguments.
template <typename T>
std::string JavaScriptPrintf(const std::string& format, const T& arg) {
  return base::StringPrintf(format.c_str(), JSONStringify(arg).c_str());
}

// Similar to above, but with an additional argument.
template <typename T1, typename T2>
std::string JavaScriptPrintf(const std::string& format, const T1& arg1,
                             const T2& arg2) {
  return base::StringPrintf(format.c_str(),
                            JSONStringify(arg1).c_str(),
                            JSONStringify(arg2).c_str());
}

// Similar to above, but with an additional argument.
template <typename T1, typename T2, typename T3>
std::string JavaScriptPrintf(const std::string& format, const T1& arg1,
                             const T2& arg2, const T3& arg3) {
  return base::StringPrintf(format.c_str(),
                            JSONStringify(arg1).c_str(),
                            JSONStringify(arg2).c_str(),
                            JSONStringify(arg3).c_str());
}

}  // namespace javascript_utils

#endif  // CHROME_TEST_AUTOMATION_JAVASCRIPT_MESSAGE_UTILS_H_
