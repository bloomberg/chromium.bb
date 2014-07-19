// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mac_util.h"

#include <string>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

using base::mac::CFCast;

namespace policy {

namespace {

// Callback function for CFDictionaryApplyFunction. |key| and |value| are an
// entry of the CFDictionary that should be converted into an equivalent entry
// in the DictionaryValue in |context|.
void DictionaryEntryToValue(const void* key, const void* value, void* context) {
  if (CFStringRef cf_key = CFCast<CFStringRef>(key)) {
    scoped_ptr<base::Value> converted =
        PropertyToValue(static_cast<CFPropertyListRef>(value));
    if (converted) {
      const std::string string = base::SysCFStringRefToUTF8(cf_key);
      static_cast<base::DictionaryValue*>(context)->Set(
          string, converted.release());
    }
  }
}

// Callback function for CFArrayApplyFunction. |value| is an entry of the
// CFArray that should be converted into an equivalent entry in the ListValue
// in |context|.
void ArrayEntryToValue(const void* value, void* context) {
  scoped_ptr<base::Value> converted =
      PropertyToValue(static_cast<CFPropertyListRef>(value));
  if (converted)
    static_cast<base::ListValue *>(context)->Append(converted.release());
}

}  // namespace

scoped_ptr<base::Value> PropertyToValue(CFPropertyListRef property) {
  if (CFCast<CFNullRef>(property))
    return scoped_ptr<base::Value>(base::Value::CreateNullValue());

  if (CFBooleanRef boolean = CFCast<CFBooleanRef>(property)) {
    return scoped_ptr<base::Value>(new base::FundamentalValue(
        static_cast<bool>(CFBooleanGetValue(boolean))));
  }

  if (CFNumberRef number = CFCast<CFNumberRef>(property)) {
    // CFNumberGetValue() converts values implicitly when the conversion is not
    // lossy. Check the type before trying to convert.
    if (CFNumberIsFloatType(number)) {
      double double_value = 0.0;
      if (CFNumberGetValue(number, kCFNumberDoubleType, &double_value)) {
        return scoped_ptr<base::Value>(
            new base::FundamentalValue(double_value));
      }
    } else {
      int int_value = 0;
      if (CFNumberGetValue(number, kCFNumberIntType, &int_value)) {
        return scoped_ptr<base::Value>(new base::FundamentalValue(int_value));
      }
    }
  }

  if (CFStringRef string = CFCast<CFStringRef>(property)) {
    return scoped_ptr<base::Value>(
        new base::StringValue(base::SysCFStringRefToUTF8(string)));
  }

  if (CFDictionaryRef dict = CFCast<CFDictionaryRef>(property)) {
    scoped_ptr<base::DictionaryValue> dict_value(new base::DictionaryValue());
    CFDictionaryApplyFunction(dict, DictionaryEntryToValue, dict_value.get());
    return dict_value.PassAs<base::Value>();
  }

  if (CFArrayRef array = CFCast<CFArrayRef>(property)) {
    scoped_ptr<base::ListValue> list_value(new base::ListValue());
    CFArrayApplyFunction(array,
                         CFRangeMake(0, CFArrayGetCount(array)),
                         ArrayEntryToValue,
                         list_value.get());
    return list_value.PassAs<base::Value>();
  }

  return scoped_ptr<base::Value>();
}

}  // namespace policy
