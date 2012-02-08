// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/utf_string_conversions.h"
#include "base/stl_util.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

// Tests for Hunspell functionality in SpellcheckingProvider
// Faked test target, which stores sent message for verification.
class TestingSpellCheckProvider : public SpellCheckProvider {
 public:
  TestingSpellCheckProvider()
      : SpellCheckProvider(NULL, NULL) {
  }

  virtual ~TestingSpellCheckProvider() {
    STLDeleteContainerPointers(messages_.begin(), messages_.end());
  }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    // Call our mock message handlers.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(TestingSpellCheckProvider, *message)
      IPC_MESSAGE_HANDLER(SpellCheckHostMsg_CallSpellingService,
                          OnCallSpellingService)
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    if (handled) {
      delete message;
      return true;
    }
    messages_.push_back(message);
    return true;
  }

  void OnCallSpellingService(int route_id,
                             int identifier,
                             int document_tag,
                             const string16& text) {
    WebKit::WebTextCheckingCompletion* completion =
        text_check_completions_.Lookup(identifier);
    if (!completion)
      return;
    text_check_completions_.Remove(identifier);
    completion->didFinishCheckingText(
        std::vector<WebKit::WebTextCheckingResult>());
  }

  std::vector<IPC::Message*> messages_;
};

namespace {

// A fake completion object for verification.
class FakeTextCheckingCompletion : public WebKit::WebTextCheckingCompletion {
 public:
  FakeTextCheckingCompletion()
      : completion_count_(0) {
  }

  virtual void didFinishCheckingText(
      const WebKit::WebVector<WebKit::WebTextCheckingResult>& results)
          OVERRIDE {
    ++completion_count_;
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

TEST_F(SpellCheckProviderTest, UsingHunspell) {
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
