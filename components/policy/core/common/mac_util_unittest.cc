// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mac_util.h"

#include <CoreFoundation/CoreFoundation.h>

#include <memory>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(PolicyMacUtilTest, PropertyToValue) {
  base::DictionaryValue root;

  // base::Value::Type::NONE
  root.Set("null", base::MakeUnique<base::Value>());

  // base::Value::Type::BOOLEAN
  root.SetBoolean("false", false);
  root.SetBoolean("true", true);

  // base::Value::Type::INTEGER
  root.SetInteger("int", 123);
  root.SetInteger("zero", 0);

  // base::Value::Type::DOUBLE
  root.SetDouble("double", 123.456);
  root.SetDouble("zerod", 0.0);

  // base::Value::Type::STRING
  root.SetString("string", "the fox jumps over something");
  root.SetString("empty", "");

  // base::Value::Type::LIST
  root.Set("emptyl", base::MakeUnique<base::Value>(base::Value::Type::LIST));
  base::ListValue list;
  for (base::DictionaryValue::Iterator it(root); !it.IsAtEnd(); it.Advance())
    list.Append(base::MakeUnique<base::Value>(it.value()));
  EXPECT_EQ(root.size(), list.GetSize());
  list.Append(base::MakeUnique<base::Value>(root));
  root.Set("list", base::MakeUnique<base::Value>(list));

  // base::Value::Type::DICTIONARY
  root.Set("emptyd",
           base::MakeUnique<base::Value>(base::Value::Type::DICTIONARY));
  // Very meta.
  root.Set("dict", base::MakeUnique<base::Value>(root));

  base::ScopedCFTypeRef<CFPropertyListRef> property(ValueToProperty(root));
  ASSERT_TRUE(property);
  std::unique_ptr<base::Value> value = PropertyToValue(property);
  ASSERT_TRUE(value);
  EXPECT_TRUE(root.Equals(value.get()));
}

}  // namespace policy
