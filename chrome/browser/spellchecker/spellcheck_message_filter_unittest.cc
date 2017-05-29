// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_message_filter.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingSpellCheckMessageFilter : public SpellCheckMessageFilter {
 public:
  TestingSpellCheckMessageFilter()
      : SpellCheckMessageFilter(0),
        spellcheck_(new SpellcheckService(&profile_)) {}

  bool Send(IPC::Message* message) override {
    sent_messages.push_back(base::WrapUnique(message));
    return true;
  }

  SpellcheckService* GetSpellcheckService() const override {
    return spellcheck_.get();
  }

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void OnTextCheckComplete(int route_id,
                           int identifier,
                           bool success,
                           const base::string16& text,
                           const std::vector<SpellCheckResult>& results) {
    SpellCheckMessageFilter::OnTextCheckComplete(route_id, identifier, success,
                                                 text, results);
  }
#endif

  std::vector<std::unique_ptr<IPC::Message>> sent_messages;

 private:
  ~TestingSpellCheckMessageFilter() override {}

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<SpellcheckService> spellcheck_;

  DISALLOW_COPY_AND_ASSIGN(TestingSpellCheckMessageFilter);
};

TEST(SpellCheckMessageFilterTest, TestOverrideThread) {
  static const uint32_t kSpellcheckMessages[] = {
    SpellCheckHostMsg_RequestDictionary::ID,
    SpellCheckHostMsg_NotifyChecked::ID,
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    SpellCheckHostMsg_CallSpellingService::ID,
#endif
  };
  content::BrowserThread::ID thread;
  IPC::Message message;
  scoped_refptr<TestingSpellCheckMessageFilter> filter(
      new TestingSpellCheckMessageFilter);
  for (size_t i = 0; i < arraysize(kSpellcheckMessages); ++i) {
    message.SetHeaderValues(
        0, kSpellcheckMessages[i], IPC::Message::PRIORITY_NORMAL);
    thread = content::BrowserThread::IO;
    filter->OverrideThreadForMessage(message, &thread);
    EXPECT_EQ(content::BrowserThread::UI, thread);
  }
}

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
TEST(SpellCheckMessageFilterTest, OnTextCheckCompleteTestCustomDictionary) {
  static const std::string kCustomWord = "Helllo";
  static const int kRouteId = 0;
  static const int kCallbackId = 0;
  static const base::string16 kText = base::ASCIIToUTF16("Helllo warld.");
  static const bool kSuccess = true;
  static const SpellCheckResult::Decoration kDecoration =
      SpellCheckResult::SPELLING;
  static const int kLocation = 7;
  static const int kLength = 5;
  static const base::string16 kReplacement = base::ASCIIToUTF16("world");

  std::vector<SpellCheckResult> results;
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 6, base::ASCIIToUTF16("Hello")));
  results.push_back(
      SpellCheckResult(kDecoration, kLocation, kLength, kReplacement));

  scoped_refptr<TestingSpellCheckMessageFilter> filter(
      new TestingSpellCheckMessageFilter);
  filter->GetSpellcheckService()->GetCustomDictionary()->AddWord(kCustomWord);
  filter->OnTextCheckComplete(kRouteId, kCallbackId, kSuccess, kText, results);
  ASSERT_EQ(1U, filter->sent_messages.size());

  SpellCheckMsg_RespondSpellingService::Param params;
  bool ok = SpellCheckMsg_RespondSpellingService::Read(
      filter->sent_messages[0].get(), &params);
  int sent_identifier = std::get<0>(params);
  bool sent_success = std::get<1>(params);
  base::string16 sent_text = std::get<2>(params);
  std::vector<SpellCheckResult> sent_results = std::get<3>(params);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kCallbackId, sent_identifier);
  EXPECT_EQ(kSuccess, sent_success);
  EXPECT_EQ(kText, sent_text);
  ASSERT_EQ(1U, sent_results.size());
  EXPECT_EQ(kDecoration, sent_results[0].decoration);
  EXPECT_EQ(kLocation, sent_results[0].location);
  EXPECT_EQ(kLength, sent_results[0].length);
  EXPECT_EQ(kReplacement, sent_results[0].replacement);
}

TEST(SpellCheckMessageFilterTest, OnTextCheckCompleteTest) {
  std::vector<SpellCheckResult> results;
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 6, base::ASCIIToUTF16("Hello")));
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 7, 7, base::ASCIIToUTF16("world")));

  scoped_refptr<TestingSpellCheckMessageFilter> filter(
      new TestingSpellCheckMessageFilter);
  filter->OnTextCheckComplete(1, 1, true, base::ASCIIToUTF16("Helllo walrd"),
                              results);
  ASSERT_EQ(1U, filter->sent_messages.size());

  SpellCheckMsg_RespondSpellingService::Param params;
  bool ok = SpellCheckMsg_RespondSpellingService::Read(
      filter->sent_messages[0].get(), &params);
  EXPECT_TRUE(ok);

  std::vector<SpellCheckResult> sent_results = std::get<3>(params);
  EXPECT_EQ(2U, sent_results.size());
}
#endif
