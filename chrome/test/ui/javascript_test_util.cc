// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/javascript_test_util.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/common/json_value_serializer.h"
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

  for (DictionaryValue::key_iterator it = dict->begin_keys();
       it != dict->end_keys(); ++it) {
    Value* value = NULL;
    bool succeeded = dict->GetWithoutPathExpansion(*it, &value);

    EXPECT_TRUE(succeeded);
    if (!succeeded)
      continue;

    const std::string& key(*it);
    std::string result;

    switch (value->GetType()) {
      case Value::TYPE_STRING:
        succeeded = value->GetAsString(&result);
        break;
      case Value::TYPE_DOUBLE: {
        double double_result;
        succeeded = value->GetAsDouble(&double_result);
        if (succeeded)
          result = base::DoubleToString(double_result);
        break;
      }
      default:
        NOTREACHED() << "Value type not supported!";
        return false;
    }

    EXPECT_TRUE(succeeded);
    if (succeeded)
      results->insert(std::make_pair(key, result));
  }

  return true;
}
