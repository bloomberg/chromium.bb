// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/js/js_event_details.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class JsEventDetailsTest : public testing::Test {};

TEST_F(JsEventDetailsTest, EmptyList) {
  JsEventDetails details;
  EXPECT_TRUE(details.Get().empty());
  EXPECT_EQ("{}", details.ToString());
}

TEST_F(JsEventDetailsTest, FromDictionary) {
  base::DictionaryValue dict;
  dict.SetString("foo", "bar");
  dict.Set("baz", new base::ListValue());

  std::unique_ptr<base::DictionaryValue> dict_copy(dict.DeepCopy());

  JsEventDetails details(&dict);

  // |details| should take over |dict|'s data.
  EXPECT_TRUE(dict.empty());
  EXPECT_TRUE(details.Get().Equals(dict_copy.get()));
}

}  // namespace
}  // namespace syncer
