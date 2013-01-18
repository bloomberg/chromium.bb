// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/javascript_test_util.h"

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

bool JsonDictionaryToMap(const std::string& json,
                         std::map<std::string, std::string>* results) {
  DCHECK(results != NULL);
  JSONStringValueSerializer deserializer(json);
  scoped_ptr<Value> root(deserializer.Deserialize(NULL, NULL));

  // Note that we don't use ASSERT_TRUE here (and in some other places) as it
  // doesn't work inside a function with a return type other than void.
  EXPECT_TRUE(root.get());
  if (!root.get())
    return false;

  EXPECT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  if (!root->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* dict = static_cast<DictionaryValue*>(root.get());

  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    double double_result;
    std::string result;
    if (it.value().GetAsString(&result)) {
    } else if (it.value().GetAsDouble(&double_result)) {
      result = base::DoubleToString(double_result);
    } else {
      NOTREACHED() << "Value type not supported!";
      return false;
    }
    results->insert(std::make_pair(it.key(), result));
  }

  return true;
}
