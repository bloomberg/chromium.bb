// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_message_filter.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/spellcheck_marker.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingSpellCheckMessageFilter : public SpellCheckMessageFilter {
 public:
  TestingSpellCheckMessageFilter()
      : SpellCheckMessageFilter(0),
        spellcheck_(new SpellcheckService(&profile_)) {}

  virtual bool Send(IPC::Message* message) OVERRIDE {
    sent_messages.push_back(message);
    return true;
  }

  virtual SpellcheckService* GetSpellcheckService() const OVERRIDE {
    return spellcheck_.get();
  }

#if !defined(OS_MACOSX)
  void OnTextCheckComplete(int route_id,
                           int identifier,
                           const std::vector<SpellCheckMarker>& markers,
                           bool success,
                           const string16& text,
                           const std::vector<SpellCheckResult>& results) {
    SpellCheckMessageFilter::OnTextCheckComplete(
        route_id, identifier, markers, success, text, results);
  }
#endif

  ScopedVector<IPC::Message> sent_messages;

 private:
  virtual ~TestingSpellCheckMessageFilter() {}

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_ptr<SpellcheckService> spellcheck_;

  DISALLOW_COPY_AND_ASSIGN(TestingSpellCheckMessageFilter);
};

TEST(SpellCheckMessageFilterTest, TestOverrideThread) {
  static const uint32 kSpellcheckMessages[] = {
    SpellCheckHostMsg_RequestDictionary::ID,
    SpellCheckHostMsg_NotifyChecked::ID,
    SpellCheckHostMsg_RespondDocumentMarkers::ID,
#if !defined(OS_MACOSX)
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

#if !defined(OS_MACOSX)
TEST(SpellCheckMessageFilterTest, OnTextCheckCompleteTestCustomDictionary) {
  static const std::string kCustomWord = "Helllo";
  static const int kRouteId = 0;
  static const int kCallbackId = 0;
  static const std::vector<SpellCheckMarker> kMarkers;
  static const string16 kText = ASCIIToUTF16("Helllo warld.");
  static const bool kSuccess = true;
  static const SpellCheckResult::Decoration kDecoration =
      SpellCheckResult::SPELLING;
  static const int kLocation = 7;
  static const int kLength = 5;
  static const string16 kReplacement = ASCIIToUTF16("world");

  std::vector<SpellCheckResult> results;
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 6, ASCIIToUTF16("Hello")));
  results.push_back(
      SpellCheckResult(kDecoration, kLocation, kLength, kReplacement));

  scoped_refptr<TestingSpellCheckMessageFilter> filter(
      new TestingSpellCheckMessageFilter);
  filter->GetSpellcheckService()->GetCustomDictionary()->AddWord(kCustomWord);
  filter->OnTextCheckComplete(
      kRouteId, kCallbackId, kMarkers, kSuccess, kText, results);
  EXPECT_EQ(static_cast<size_t>(1), filter->sent_messages.size());

  int sent_identifier = -1;
  bool sent_success = false;
  string16 sent_text;
  std::vector<SpellCheckResult> sent_results;
  bool ok = SpellCheckMsg_RespondSpellingService::Read(filter->sent_messages[0],
                                                       &sent_identifier,
                                                       &sent_success,
                                                       &sent_text,
                                                       &sent_results);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kCallbackId, sent_identifier);
  EXPECT_EQ(kSuccess, sent_success);
  EXPECT_EQ(kText, sent_text);
  EXPECT_EQ(static_cast<size_t>(1), sent_results.size());
  EXPECT_EQ(kDecoration, sent_results[0].decoration);
  EXPECT_EQ(kLocation, sent_results[0].location);
  EXPECT_EQ(kLength, sent_results[0].length);
  EXPECT_EQ(kReplacement, sent_results[0].replacement);
}

TEST(SpellCheckMessageFilterTest, OnTextCheckCompleteTest) {
  std::vector<SpellCheckResult> results;
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 6, ASCIIToUTF16("Hello")));
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 7, 7, ASCIIToUTF16("world")));

  scoped_refptr<TestingSpellCheckMessageFilter> filter(
      new TestingSpellCheckMessageFilter);
  filter->OnTextCheckComplete(1, 1, std::vector<SpellCheckMarker>(),
      true,  ASCIIToUTF16("Helllo walrd"), results);
  EXPECT_EQ(static_cast<size_t>(1), filter->sent_messages.size());

  int sent_identifier = -1;
  bool sent_success = false;
  string16 sent_text;
  std::vector<SpellCheckResult> sent_results;
  bool ok = SpellCheckMsg_RespondSpellingService::Read(filter->sent_messages[0],
                                                       &sent_identifier,
                                                       &sent_success,
                                                       &sent_text,
                                                       &sent_results);
  EXPECT_TRUE(ok);
  EXPECT_EQ(static_cast<size_t>(2), sent_results.size());
}
#endif
