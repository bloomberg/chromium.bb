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
  EXPECT_EQ("[]", arg_list.ToString());
}

TEST_F(JsArgListTest, FromList) {
  scoped_ptr<ListValue> list(new ListValue());
  list->Append(Value::CreateBooleanValue(false));
  list->Append(Value::CreateIntegerValue(5));
  DictionaryValue* dict = new DictionaryValue();
  list->Append(dict);
  dict->SetString("foo", "bar");
  dict->Set("baz", new ListValue());

  scoped_ptr<ListValue> list_copy(list->DeepCopy());

  JsArgList arg_list(list.get());

  // |arg_list| should take over |list|'s data.
  EXPECT_TRUE(list->empty());
  EXPECT_TRUE(arg_list.Get().Equals(list_copy.get()));
}

}  // namespace
}  // namespace browser_sync
