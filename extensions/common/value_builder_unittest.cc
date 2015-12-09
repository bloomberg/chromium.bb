// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ValueBuilderTest = testing::Test;

namespace extensions {

TEST(ValueBuilderTest, Basic) {
  ListBuilder permission_list;
  permission_list.Append("tabs").Append("history");

  scoped_ptr<base::DictionaryValue> settings(new base::DictionaryValue);

  ASSERT_FALSE(settings->GetList("permissions", nullptr));
  settings = DictionaryBuilder()
                 .Set("permissions", std::move(permission_list))
                 .Build();
  base::ListValue* list_value;
  ASSERT_TRUE(settings->GetList("permissions", &list_value));

  ASSERT_EQ(2U, list_value->GetSize());
  std::string permission;
  ASSERT_TRUE(list_value->GetString(0, &permission));
  ASSERT_EQ(permission, "tabs");
  ASSERT_TRUE(list_value->GetString(1, &permission));
  ASSERT_EQ(permission, "history");
}

}  // namespace extensions
