// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/utf_string_conversions.h"
#include "base/stl_util.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/renderer/spellchecker/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

// Tests for Hunspell functionality in SpellcheckingProvider

namespace {

TEST_F(SpellCheckProviderTest, UsingHunspell) {
  FakeTextCheckingCompletion completion;
  provider_.RequestTextChecking(WebKit::WebString("hello"),
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
  provider_.RequestTextChecking(WebKit::WebString(), &completion);
  EXPECT_EQ(-1, provider_.offset_);
  EXPECT_TRUE(provider_.text_.empty());

  // Verify that the SpellCheckProvider class does not spellcheck text while we
  // are typing a word.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First"), &completion);
  EXPECT_EQ(-1, provider_.offset_);
  EXPECT_TRUE(provider_.text_.empty());

  // Verify that the SpellCheckProvider class spellcheck the first word when we
  // type a space key, i.e. when we finish typing a word.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First "), &completion);
  EXPECT_EQ(0, provider_.offset_);
  EXPECT_EQ(ASCIIToUTF16("First "), provider_.text_);

  // Verify that the SpellCheckProvider class spellcheck the first line when we
  // type a return key, i.e. when we finish typing a line.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First Second\n"),
                                &completion);
  EXPECT_EQ(0, provider_.offset_);
  EXPECT_EQ(ASCIIToUTF16("First Second\n"), provider_.text_);

  // Verify that the SpellCheckProvider class spellcheck the lines when we
  // finish typing a word "Third" to the second line.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First Second\nThird "),
                                &completion);
  EXPECT_EQ(0, provider_.offset_);
  EXPECT_EQ(ASCIIToUTF16("First Second\nThird "), provider_.text_);

  // Verify that the SpellCheckProvider class does not send a spellcheck request
  // when a user inserts whitespace characters.
  provider_.ResetResult();
  provider_.RequestTextChecking(WebKit::WebString("First Second\nThird   "),
                                &completion);
  EXPECT_EQ(-1, provider_.offset_);
  EXPECT_TRUE(provider_.text_.empty());

  // Verify that the SpellCheckProvider class spellcheck the lines when we type
  // a period.
  provider_.ResetResult();
  provider_.RequestTextChecking(
      WebKit::WebString("First Second\nThird   Fourth."), &completion);
  EXPECT_EQ(0, provider_.offset_);
  EXPECT_EQ(ASCIIToUTF16("First Second\nThird   Fourth."), provider_.text_);
}

// Tests that the SpellCheckProvider class cancels incoming spellcheck requests
// when it does not need to handle them.
TEST_F(SpellCheckProviderTest, CancelUnnecessaryRequests) {
  FakeTextCheckingCompletion completion;
  provider_.RequestTextChecking(WebKit::WebString("hello."),
                                &completion);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);

  // Test that the SpellCheckProvider class cancels an incoming request with the
  // text same as above.
  provider_.RequestTextChecking(WebKit::WebString("hello."),
                                &completion);
  EXPECT_EQ(completion.completion_count_, 2U);
  EXPECT_EQ(completion.cancellation_count_, 1U);

  // Test that the SpellCheckProvider class cancels an incoming request that
  // does not include any words.
  provider_.RequestTextChecking(WebKit::WebString(":-)"),
                                &completion);
  EXPECT_EQ(completion.completion_count_, 3U);
  EXPECT_EQ(completion.cancellation_count_, 2U);

  // Test that the SpellCheckProvider class sends a request when it receives a
  // Russian word.
  const wchar_t kRussianWord[] = L"\x0431\x0451\x0434\x0440\x0430";
  provider_.RequestTextChecking(WebKit::WebString(
      base::WideToUTF16(kRussianWord)), &completion);
  EXPECT_EQ(completion.completion_count_, 4U);
  EXPECT_EQ(completion.cancellation_count_, 2U);
}

}  // namespace
