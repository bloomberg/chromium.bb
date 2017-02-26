// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SpellCheckProviderMacTest : public SpellCheckProviderTest {};

void FakeMessageArrival(
    SpellCheckProvider* provider,
    const SpellCheckHostMsg_RequestTextCheck::Param& parameters) {
  std::vector<SpellCheckResult> fake_result;
  bool handled = provider->OnMessageReceived(
      SpellCheckMsg_RespondTextCheck(
          0,
          std::get<1>(parameters),
          base::ASCIIToUTF16("test"),
          fake_result));
  EXPECT_TRUE(handled);
}

TEST_F(SpellCheckProviderMacTest, SingleRoundtripSuccess) {
  FakeTextCheckingCompletion completion;

  provider_.RequestTextChecking(base::ASCIIToUTF16("hello "), &completion);
  EXPECT_EQ(completion.completion_count_, 0U);
  EXPECT_EQ(provider_.messages_.size(), 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 1U);

  SpellCheckHostMsg_RequestTextCheck::Param read_parameters1;
  bool ok = SpellCheckHostMsg_RequestTextCheck::Read(
      provider_.messages_[0].get(), &read_parameters1);
  EXPECT_TRUE(ok);
  EXPECT_EQ(std::get<2>(read_parameters1), base::ASCIIToUTF16("hello "));

  FakeMessageArrival(&provider_, read_parameters1);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);
}

TEST_F(SpellCheckProviderMacTest, TwoRoundtripSuccess) {
  FakeTextCheckingCompletion completion1;
  provider_.RequestTextChecking(base::ASCIIToUTF16("hello "), &completion1);
  FakeTextCheckingCompletion completion2;
  provider_.RequestTextChecking(base::ASCIIToUTF16("bye "), &completion2);

  EXPECT_EQ(completion1.completion_count_, 0U);
  EXPECT_EQ(completion2.completion_count_, 0U);
  EXPECT_EQ(provider_.messages_.size(), 2U);
  EXPECT_EQ(provider_.pending_text_request_size(), 2U);

  SpellCheckHostMsg_RequestTextCheck::Param read_parameters1;
  bool ok = SpellCheckHostMsg_RequestTextCheck::Read(
      provider_.messages_[0].get(), &read_parameters1);
  EXPECT_TRUE(ok);
  EXPECT_EQ(std::get<2>(read_parameters1), base::UTF8ToUTF16("hello "));

  SpellCheckHostMsg_RequestTextCheck::Param read_parameters2;
  ok = SpellCheckHostMsg_RequestTextCheck::Read(provider_.messages_[1].get(),
                                                &read_parameters2);
  EXPECT_TRUE(ok);
  EXPECT_EQ(std::get<2>(read_parameters2), base::UTF8ToUTF16("bye "));

  FakeMessageArrival(&provider_, read_parameters1);
  EXPECT_EQ(completion1.completion_count_, 1U);
  EXPECT_EQ(completion2.completion_count_, 0U);
  EXPECT_EQ(provider_.pending_text_request_size(), 1U);

  FakeMessageArrival(&provider_, read_parameters2);
  EXPECT_EQ(completion1.completion_count_, 1U);
  EXPECT_EQ(completion2.completion_count_, 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);
}

}  // namespace
