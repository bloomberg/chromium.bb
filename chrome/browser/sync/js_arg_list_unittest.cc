// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_arg_list.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

class JsArgListTest : public testing::Test {};

TEST_F(JsArgListTest, EmptyList) {
  JsArgList arg_list;
  EXPECT_TRUE(arg_list.Get().empty());
}

TEST_F(JsArgListTest, FromList) {
  scoped_ptr<ListValue> list(new ListValue());
  list->Append(Value::CreateBooleanValue(false));
  list->Append(Value::CreateIntegerValue(5));
  DictionaryValue* dict = new DictionaryValue();
  list->Append(dict);
  dict->SetString("foo", "bar");
  dict->Set("baz", new ListValue());

  JsArgList arg_list(*list);

  // Make sure arg_list takes a deep copy.
  scoped_ptr<ListValue> list_copy(list->DeepCopy());
  list.reset();
  EXPECT_TRUE(arg_list.Get().Equals(list_copy.get()));
}

TEST_F(JsArgListTest, FromVector) {
  FundamentalValue bool_value(false);
  FundamentalValue int_value(5);
  DictionaryValue dict;
  dict.SetString("foo", "bar");
  dict.Set("baz", new ListValue());

  std::vector<const Value*> vec;
  vec.push_back(&bool_value);
  vec.push_back(&int_value);
  vec.push_back(&dict);

  JsArgList arg_list(vec);

  ListValue list;
  list.Append(bool_value.DeepCopy());
  list.Append(int_value.DeepCopy());
  list.Append(dict.DeepCopy());

  // Make sure arg_list takes a deep copy.
  vec.clear();
  dict.SetString("baz", "foo");
  EXPECT_TRUE(arg_list.Get().Equals(&list));
}

}  // namespace
}  // namespace browser_sync
