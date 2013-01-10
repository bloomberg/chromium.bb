// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/renderer/spellchecker/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

namespace {

class SpellCheckProviderMacTest : public SpellCheckProviderTest {};

struct MessageParameters {
  MessageParameters()
      : router_id(0),
        request_id(0) {}

  int router_id;
  int request_id;
  string16 text;
};

MessageParameters ReadRequestTextCheck(IPC::Message* message) {
  MessageParameters parameters;
  bool ok = SpellCheckHostMsg_RequestTextCheck::Read(
      message,
      &parameters.router_id,
      &parameters.request_id,
      &parameters.text);
  EXPECT_TRUE(ok);
  return parameters;
}

void FakeMessageArrival(SpellCheckProvider* provider,
                        const MessageParameters& parameters) {
  std::vector<SpellCheckResult> fake_result;
  bool handled = provider->OnMessageReceived(
      SpellCheckMsg_RespondTextCheck(
          0,
          parameters.request_id,
          fake_result));
  EXPECT_TRUE(handled);
}

TEST_F(SpellCheckProviderMacTest, SingleRoundtripSuccess) {
  FakeTextCheckingCompletion completion;

  provider_.RequestTextChecking(WebKit::WebString("hello"),
                                &completion);
  EXPECT_EQ(completion.completion_count_, 0U);
  EXPECT_EQ(provider_.messages_.size(), 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 1U);

  MessageParameters read_parameters =
      ReadRequestTextCheck(provider_.messages_[0]);
  EXPECT_EQ(read_parameters.text, UTF8ToUTF16("hello"));

  FakeMessageArrival(&provider_, read_parameters);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);
}

TEST_F(SpellCheckProviderMacTest, TwoRoundtripSuccess) {
  FakeTextCheckingCompletion completion1;
  provider_.RequestTextChecking(WebKit::WebString("hello"),
                                &completion1);
  FakeTextCheckingCompletion completion2;
  provider_.RequestTextChecking(WebKit::WebString("bye"),
                                &completion2);

  EXPECT_EQ(completion1.completion_count_, 0U);
  EXPECT_EQ(completion2.completion_count_, 0U);
  EXPECT_EQ(provider_.messages_.size(), 2U);
  EXPECT_EQ(provider_.pending_text_request_size(), 2U);

  MessageParameters read_parameters1 =
      ReadRequestTextCheck(provider_.messages_[0]);
  EXPECT_EQ(read_parameters1.text, UTF8ToUTF16("hello"));

  MessageParameters read_parameters2 =
      ReadRequestTextCheck(provider_.messages_[1]);
  EXPECT_EQ(read_parameters2.text, UTF8ToUTF16("bye"));

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
