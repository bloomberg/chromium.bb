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
      : SpellCheckProvider(NULL, NULL),
        offset_(-1) {
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
                             int offset,
                             const string16& text) {
    WebKit::WebTextCheckingCompletion* completion =
        text_check_completions_.Lookup(identifier);
    if (!completion) {
      ResetResult();
      return;
    }
    offset_ = offset;
    text_.assign(text);
    text_check_completions_.Remove(identifier);
    completion->didFinishCheckingText(
        std::vector<WebKit::WebTextCheckingResult>());
    last_request_ = text;
  }

  void ResetResult() {
    offset_ = -1;
    text_.clear();
  }

  int offset_;
  string16 text_;
  std::vector<IPC::Message*> messages_;
};

namespace {

// A fake completion object for verification.
class FakeTextCheckingCompletion : public WebKit::WebTextCheckingCompletion {
 public:
  FakeTextCheckingCompletion()
      : completion_count_(0),
        cancellation_count_(0) {
  }

  virtual void didFinishCheckingText(
      const WebKit::WebVector<WebKit::WebTextCheckingResult>& results)
          OVERRIDE {
    ++completion_count_;
    last_results_ = results;
  }

  virtual void didCancelCheckingText() OVERRIDE {
    ++completion_count_;
    ++cancellation_count_;
  }

  size_t completion_count_;
  size_t cancellation_count_;
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

// Tests that the SpellCheckProvider object sends a spellcheck request when a
// user finishes typing a word. Also this test verifies that this object checks
// only a line being edited by the user.
TEST_F(SpellCheckProviderTest, MultiLineText) {
  FakeTextCheckingCompletion completion;

  // Verify that the SpellCheckProvider class does not spellcheck empty text.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString(), 0, &completion);
  EXPECT_EQ(-1, provider_.offset_);
  EXPECT_TRUE(provider_.text_.empty());

  // Verify that the SpellCheckProvider class does not spellcheck text while we
  // are typing a word.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First"), 0, &completion);
  EXPECT_EQ(-1, provider_.offset_);
  EXPECT_TRUE(provider_.text_.empty());

  // Verify that the SpellCheckProvider class spellcheck the first word when we
  // type a space key, i.e. when we finish typing a word.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First "), 0, &completion);
  EXPECT_EQ(0, provider_.offset_);
  EXPECT_EQ(ASCIIToUTF16("First "), provider_.text_);

  // Verify that the SpellCheckProvider class spellcheck the first line when we
  // type a return key, i.e. when we finish typing a line.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First Second\n"), 0,
                                &completion);
  EXPECT_EQ(0, provider_.offset_);
  EXPECT_EQ(ASCIIToUTF16("First Second\n"), provider_.text_);

  // Verify that the SpellCheckProvider class spellcheck the lines when we
  // finish typing a word "Third" to the second line.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First Second\nThird "), 0,
                                &completion);
  EXPECT_EQ(0, provider_.offset_);
  EXPECT_EQ(ASCIIToUTF16("First Second\nThird "), provider_.text_);

  // Verify that the SpellCheckProvider class does not send a spellcheck request
  // when a user inserts whitespace characters.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First Second\nThird   "), 0,
                                &completion);
  EXPECT_EQ(-1, provider_.offset_);
  EXPECT_TRUE(provider_.text_.empty());

  // Verify that the SpellCheckProvider class spellcheck the lines when we type
  // a period.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      WebKit::WebString("First Second\nThird   Fourth."), 0, &completion);
  EXPECT_EQ(0, provider_.offset_);
  EXPECT_EQ(ASCIIToUTF16("First Second\nThird   Fourth."), provider_.text_);
}

// Tests that the SpellCheckProvider class cancels incoming spellcheck requests
// when it does not need to handle them.
TEST_F(SpellCheckProviderTest,CancelUnnecessaryRequests) {
  int document_tag = 123;
  FakeTextCheckingCompletion completion;
  provider_.RequestTextChecking(WebKit::WebString("hello."),
                                document_tag,
                                &completion);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);

  // Test that the SpellCheckProvider class cancels an incoming request with the
  // text same as above.
  provider_.RequestTextChecking(WebKit::WebString("hello."),
                                document_tag,
                                &completion);
  EXPECT_EQ(completion.completion_count_, 2U);
  EXPECT_EQ(completion.cancellation_count_, 1U);

  // Test that the SpellCheckProvider class cancels an incoming request that
  // does not include any words.
  provider_.RequestTextChecking(WebKit::WebString(":-)"),
                                document_tag,
                                &completion);
  EXPECT_EQ(completion.completion_count_, 3U);
  EXPECT_EQ(completion.cancellation_count_, 2U);
}

}  // namespace
