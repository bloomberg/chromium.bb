// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/macros.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

TEST(SelectorTest, FromProto) {
  ElementReferenceProto proto;
  proto.add_selectors("a");
  proto.add_selectors("b");
  proto.set_inner_text_pattern("c");
  proto.set_pseudo_type(PseudoType::BEFORE);

  Selector selector(proto);
  EXPECT_THAT(selector.selectors, testing::ElementsAre("a", "b"));
  EXPECT_EQ("c", selector.inner_text_pattern);
  EXPECT_EQ(PseudoType::BEFORE, selector.pseudo_type);
}

TEST(SelectorTest, Comparison) {
  EXPECT_FALSE(Selector({"a"}) == Selector({"b"}));
  EXPECT_LT(Selector({"a"}), Selector({"b"}));
  EXPECT_TRUE(Selector({"a"}) == Selector({"a"}));

  EXPECT_FALSE(Selector({"a"}, PseudoType::BEFORE) ==
               Selector({"a"}, PseudoType::AFTER));
  EXPECT_LT(Selector({"a"}, PseudoType::BEFORE),
            Selector({"a"}, PseudoType::AFTER));
  EXPECT_LT(Selector({"a"}, PseudoType::BEFORE), Selector({"b"}));
  EXPECT_TRUE(Selector({"a"}, PseudoType::BEFORE) ==
              Selector({"a"}, PseudoType::BEFORE));

  EXPECT_FALSE(Selector({"a"}).MatchingInnerText("a") ==
               Selector({"a"}).MatchingInnerText("b"));
  EXPECT_LT(Selector({"a"}).MatchingInnerText("a"),
            Selector({"a"}).MatchingInnerText("b"));
  EXPECT_TRUE(Selector({"a"}).MatchingInnerText("a") ==
              Selector({"a"}).MatchingInnerText("a"));
}

}  // namespace
}  // namespace autofill_assistant
