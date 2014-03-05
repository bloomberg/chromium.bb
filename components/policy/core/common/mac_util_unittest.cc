// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/policy/core/common/mac_util.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(PolicyMacUtilTest, PropertyToValue) {
  base::DictionaryValue root;

  // base::Value::TYPE_NULL
  root.Set("null", base::Value::CreateNullValue());

  // base::Value::TYPE_BOOLEAN
  root.SetBoolean("false", false);
  root.SetBoolean("true", true);

  // base::Value::TYPE_INTEGER
  root.SetInteger("int", 123);
  root.SetInteger("zero", 0);

  // base::Value::TYPE_DOUBLE
  root.SetDouble("double", 123.456);
  root.SetDouble("zerod", 0.0);

  // base::Value::TYPE_STRING
  root.SetString("string", "the fox jumps over something");
  root.SetString("empty", "");

  // base::Value::TYPE_LIST
  base::ListValue list;
  root.Set("emptyl", list.DeepCopy());
  for (base::DictionaryValue::Iterator it(root); !it.IsAtEnd(); it.Advance())
    list.Append(it.value().DeepCopy());
  EXPECT_EQ(root.size(), list.GetSize());
  list.Append(root.DeepCopy());
  root.Set("list", list.DeepCopy());

  // base::Value::TYPE_DICTIONARY
  base::DictionaryValue dict;
  root.Set("emptyd", dict.DeepCopy());
  // Very meta.
  root.Set("dict", root.DeepCopy());

  base::ScopedCFTypeRef<CFPropertyListRef> property(ValueToProperty(&root));
  ASSERT_TRUE(property);
  scoped_ptr<base::Value> value = PropertyToValue(property);
  ASSERT_TRUE(value);
  EXPECT_TRUE(root.Equals(value.get()));
}

}  // namespace policy
