// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

namespace {

// Faked test target, which stores sent message for verification,
// and allows manipulate |is_using_platform_spelling_engine| parameter.
class TestingSpellCheckProvider : public SpellCheckProvider {
 public:
  TestingSpellCheckProvider()
      : SpellCheckProvider(NULL, NULL),
        is_using_platform_spelling_engine_(true) {
  }

  virtual ~TestingSpellCheckProvider() {
    for (std::vector<IPC::Message*>::iterator i = messages_.begin();
         i != messages_.end();
         ++i) {
      delete *i;
    }
  }

  virtual bool Send(IPC::Message* message) {
    messages_.push_back(message);
    return true;
  }

  virtual bool is_using_platform_spelling_engine() const {
    return is_using_platform_spelling_engine_;
  }

  std::vector<IPC::Message*> messages_;
  bool is_using_platform_spelling_engine_;
};

// A fake completion object for verification.
class FakeTextCheckingCompletion : public WebKit::WebTextCheckingCompletion {
 public:
  FakeTextCheckingCompletion()
      : completion_count_(0) {
  }

  virtual void didFinishCheckingText(
      const WebKit::WebVector<WebKit::WebTextCheckingResult>& results) {
    completion_count_++;
    last_results_ = results;
  }

  size_t completion_count_;
  WebKit::WebVector<WebKit::WebTextCheckingResult> last_results_;
};

class SpellCheckProviderTest : public testing::Test {
 public:
  SpellCheckProviderTest() { }
  virtual ~SpellCheckProviderTest() { }

 protected:
  TestingSpellCheckProvider provider_;
};

struct MessageParameters {
  MessageParameters()
      : router_id(0),
        request_id(0),
        document_tag(0) { }

  int router_id;
  int request_id;
  int document_tag;
  string16 text;
};

MessageParameters ReadPlatformRequestTextCheck(IPC::Message* message) {
  MessageParameters parameters;
  bool ok = SpellCheckHostMsg_PlatformRequestTextCheck::Read(
      message,
      &parameters.router_id,
      &parameters.request_id,
      &parameters.document_tag,
      &parameters.text);
  EXPECT_TRUE(ok);
  return parameters;
}

void FakeMessageArrival(SpellCheckProvider* provider,
                        const MessageParameters& parameters) {
  std::vector<WebKit::WebTextCheckingResult> fake_result;
  bool handled = provider->OnMessageReceived(
      SpellCheckMsg_RespondTextCheck(
          0,
          parameters.request_id,
          parameters.document_tag,
          fake_result));
  EXPECT_TRUE(handled);
}

TEST_F(SpellCheckProviderTest, SingleRoundtripSuccess) {
  FakeTextCheckingCompletion completion;
  int document_tag = 123;

  provider_.RequestTextChecking(WebKit::WebString("hello"),
                                document_tag,
                                &completion);
  EXPECT_EQ(completion.completion_count_, 0U);
  EXPECT_EQ(provider_.messages_.size(), 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 1U);

  MessageParameters read_parameters =
      ReadPlatformRequestTextCheck(provider_.messages_[0]);
  EXPECT_EQ(read_parameters.text, UTF8ToUTF16("hello"));

  FakeMessageArrival(&provider_, read_parameters);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);
}

TEST_F(SpellCheckProviderTest, TwoRoundtripSuccess) {
  int document_tag = 123;

  FakeTextCheckingCompletion completion1;
  provider_.RequestTextChecking(WebKit::WebString("hello"),
                                document_tag,
                                &completion1);
  FakeTextCheckingCompletion completion2;
  provider_.RequestTextChecking(WebKit::WebString("bye"),
                                document_tag,
                                &completion2);

  EXPECT_EQ(completion1.completion_count_, 0U);
  EXPECT_EQ(completion2.completion_count_, 0U);
  EXPECT_EQ(provider_.messages_.size(), 2U);
  EXPECT_EQ(provider_.pending_text_request_size(), 2U);

  MessageParameters read_parameters1 =
      ReadPlatformRequestTextCheck(provider_.messages_[0]);
  EXPECT_EQ(read_parameters1.text, UTF8ToUTF16("hello"));

  MessageParameters read_parameters2 =
      ReadPlatformRequestTextCheck(provider_.messages_[1]);
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

TEST_F(SpellCheckProviderTest, PlatformEngineUnavailable) {
  provider_.is_using_platform_spelling_engine_ = false;

  int document_tag = 123;
  FakeTextCheckingCompletion completion;
  provider_.RequestTextChecking(WebKit::WebString("hello"),
                                document_tag,
                                &completion);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(provider_.messages_.size(), 0U);
  EXPECT_EQ(provider_.pending_text_request_size(), 0U);
}

}  // namespace
