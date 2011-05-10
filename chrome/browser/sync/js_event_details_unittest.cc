// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_event_details.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

class JsEventDetailsTest : public testing::Test {};

TEST_F(JsEventDetailsTest, EmptyList) {
  JsEventDetails details;
  EXPECT_TRUE(details.Get().empty());
  EXPECT_EQ("{}", details.ToString());
}

TEST_F(JsEventDetailsTest, FromDictionary) {
  DictionaryValue dict;
  dict.SetString("foo", "bar");
  dict.Set("baz", new ListValue());

  scoped_ptr<DictionaryValue> dict_copy(dict.DeepCopy());

  JsEventDetails details(&dict);

  // |details| should take over |dict|'s data.
  EXPECT_TRUE(dict.empty());
  EXPECT_TRUE(details.Get().Equals(dict_copy.get()));
}

}  // namespace
}  // namespace browser_sync
