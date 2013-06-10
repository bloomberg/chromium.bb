// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/spellchecker/feedback_sender.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/entropy_provider.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_marker.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace spellcheck {

namespace {

static const int kMisspellingLength = 6;
static const int kMisspellingStart = 0;
static const int kRendererProcessId = 0;
static const int kUrlFetcherId = 0;
static const std::string kCountry = "USA";
static const std::string kLanguage = "en";
static const string16 kText = ASCIIToUTF16("Helllo world.");

SpellCheckResult BuildSpellCheckResult() {
  return SpellCheckResult(SpellCheckResult::SPELLING,
                          kMisspellingStart,
                          kMisspellingLength,
                          ASCIIToUTF16("Hello"));
}

}  // namespace

class FeedbackSenderTest : public testing::Test {
 public:
  FeedbackSenderTest() : ui_thread_(content::BrowserThread::UI, &loop_) {

    // The command-line switch and the field trial are temporary.
    // TODO(rouslan): Remove the command-line switch and the field trial.
    // http://crbug.com/247726
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableSpellingServiceFeedback);
    field_trial_list_.reset(
        new base::FieldTrialList(new metrics::SHA1EntropyProvider("foo")));
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        kFeedbackFieldTrialName, kFeedbackFieldTrialEnabledGroupName);
    field_trial_->group();

    feedback_.reset(new FeedbackSender(NULL, kLanguage, kCountry));
  }
  virtual ~FeedbackSenderTest() {}

 private:
  TestingProfile profile_;
  base::MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<base::FieldTrialList> field_trial_list_;
  scoped_refptr<base::FieldTrial> field_trial_;

 protected:
  uint32 AddPendingFeedback() {
    std::vector<SpellCheckResult> results(1, BuildSpellCheckResult());
    feedback_->OnSpellcheckResults(
        &results, kRendererProcessId, kText, std::vector<SpellCheckMarker>());
    return results[0].hash;
  }

  void ExpireSession() {
    feedback_->session_start_ =
        base::Time::Now() -
        base::TimeDelta::FromHours(chrome::spellcheck_common::kSessionHours);
  }

  net::TestURLFetcher* GetFetcher() {
    return fetchers_.GetFetcherByID(kUrlFetcherId);
  }

  net::TestURLFetcherFactory fetchers_;
  scoped_ptr<spellcheck::FeedbackSender> feedback_;
};

// Do not send data if there's no feedback.
TEST_F(FeedbackSenderTest, NoFeedback) {
  EXPECT_EQ(NULL, GetFetcher());
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  EXPECT_EQ(NULL, GetFetcher());
}

// Do not send data if not aware of which markers are still in the document.
TEST_F(FeedbackSenderTest, NoDocumentMarkersReceived) {
  EXPECT_EQ(NULL, GetFetcher());
  uint32 hash = AddPendingFeedback();
  EXPECT_EQ(NULL, GetFetcher());
  static const int kSuggestionIndex = 1;
  feedback_->SelectedSuggestion(hash, kSuggestionIndex);
  EXPECT_EQ(NULL, GetFetcher());
}

// Send PENDING feedback message if the marker is still in the document, and the
// user has not performed any action on it.
TEST_F(FeedbackSenderTest, PendingFeedback) {
  uint32 hash = AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>(1, hash));
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"PENDING\""))
      << fetcher->upload_data();
}

// Send NO_ACTION feedback message if the marker has been removed from the
// document.
TEST_F(FeedbackSenderTest, NoActionFeedback) {
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"NO_ACTION\""))
      << fetcher->upload_data();
}

// Send SELECT feedback message if the user has selected a spelling suggestion.
TEST_F(FeedbackSenderTest, SelectFeedback) {
  uint32 hash = AddPendingFeedback();
  static const int kSuggestion = 0;
  feedback_->SelectedSuggestion(hash, kSuggestion);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"SELECT\""))
      << fetcher->upload_data();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionTargetIndex\":" + kSuggestion))
      << fetcher->upload_data();
}

// Send ADD_TO_DICT feedback message if the user has added the misspelled word
// to the custom dictionary.
TEST_F(FeedbackSenderTest, AddToDictFeedback) {
  uint32 hash = AddPendingFeedback();
  feedback_->AddedToDictionary(hash);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"ADD_TO_DICT\""))
      << fetcher->upload_data();
}

// Send PENDING feedback message if the user saw the spelling suggestion, but
// decided to not select it, and the marker is still in the document.
TEST_F(FeedbackSenderTest, IgnoreFeedbackMarkerInDocument) {
  uint32 hash = AddPendingFeedback();
  feedback_->IgnoredSuggestions(hash);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>(1, hash));
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"PENDING\""))
      << fetcher->upload_data();
}

// Send IGNORE feedback message if the user saw the spelling suggestion, but
// decided to not select it, and the marker is no longer in the document.
TEST_F(FeedbackSenderTest, IgnoreFeedbackMarkerNotInDocument) {
  uint32 hash = AddPendingFeedback();
  feedback_->IgnoredSuggestions(hash);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"IGNORE\""))
      << fetcher->upload_data();
}

// Send MANUALLY_CORRECTED feedback message if the user manually corrected the
// misspelled word.
TEST_F(FeedbackSenderTest, ManuallyCorrectedFeedback) {
  uint32 hash = AddPendingFeedback();
  static const std::string kManualCorrection = "Howdy";
  feedback_->ManuallyCorrected(hash, ASCIIToUTF16(kManualCorrection));
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(
      std::string::npos,
      fetcher->upload_data().find("\"actionType\":\"MANUALLY_CORRECTED\""))
      << fetcher->upload_data();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionTargetValue\":\"" +
                                        kManualCorrection + "\""))
      << fetcher->upload_data();
}

// Send feedback messages in batch.
TEST_F(FeedbackSenderTest, BatchFeedback) {
  std::vector<SpellCheckResult> results;
  results.push_back(SpellCheckResult(SpellCheckResult::SPELLING,
                                     kMisspellingStart,
                                     kMisspellingLength,
                                     ASCIIToUTF16("Hello")));
  static const int kSecondMisspellingStart = 7;
  static const int kSecondMisspellingLength = 5;
  results.push_back(SpellCheckResult(SpellCheckResult::SPELLING,
                                     kSecondMisspellingStart,
                                     kSecondMisspellingLength,
                                     ASCIIToUTF16("world")));
  feedback_->OnSpellcheckResults(
      &results, kRendererProcessId, kText, std::vector<SpellCheckMarker>());
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  net::TestURLFetcher* fetcher = GetFetcher();
  size_t pos = fetcher->upload_data().find("\"actionType\":\"NO_ACTION\"");
  EXPECT_NE(std::string::npos, pos) << fetcher->upload_data();
  pos = fetcher->upload_data().find("\"actionType\":\"NO_ACTION\"", pos + 1);
  EXPECT_NE(std::string::npos, pos) << fetcher->upload_data();
}

// Send a series of PENDING feedback messages and one final NO_ACTION feedback
// message with the same hash identifier for a single misspelling.
TEST_F(FeedbackSenderTest, SameHashFeedback) {
  uint32 hash = AddPendingFeedback();
  std::vector<uint32> remaining_markers(1, hash);

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"PENDING\""))
      << fetcher->upload_data();
  std::string hash_string = base::StringPrintf("\"suggestionId\":\"%u\"", hash);
  EXPECT_NE(std::string::npos, fetcher->upload_data().find(hash_string))
      << fetcher->upload_data();
  fetchers_.RemoveFetcherFromMap(kUrlFetcherId);

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"PENDING\""))
      << fetcher->upload_data();
  EXPECT_NE(std::string::npos, fetcher->upload_data().find(hash_string))
      << fetcher->upload_data();
  fetchers_.RemoveFetcherFromMap(kUrlFetcherId);

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"NO_ACTION\""))
      << fetcher->upload_data();
  EXPECT_NE(std::string::npos, fetcher->upload_data().find(hash_string))
      << fetcher->upload_data();
  fetchers_.RemoveFetcherFromMap(kUrlFetcherId);

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  EXPECT_EQ(NULL, GetFetcher());
}

// When a session expires:
// 1) Pending feedback is finalized and sent to the server in the last message
//    batch in the session.
// 2) No feedback is sent until a spellcheck request happens.
// 3) Existing markers get new hash identifiers.
TEST_F(FeedbackSenderTest, SessionExpirationFeedback) {
  std::vector<SpellCheckResult> results(
      1,
      SpellCheckResult(SpellCheckResult::SPELLING,
                       kMisspellingStart,
                       kMisspellingLength,
                       ASCIIToUTF16("Hello")));
  feedback_->OnSpellcheckResults(
      &results, kRendererProcessId, kText, std::vector<SpellCheckMarker>());
  uint32 original_hash = results[0].hash;
  std::vector<uint32> remaining_markers(1, original_hash);

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_EQ(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"NO_ACTION\""))
      << fetcher->upload_data();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"PENDING\""))
      << fetcher->upload_data();
  std::string original_hash_string =
      base::StringPrintf("\"suggestionId\":\"%u\"", original_hash);
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find(original_hash_string))
      << fetcher->upload_data();
  fetchers_.RemoveFetcherFromMap(kUrlFetcherId);

  ExpireSession();

  // Last message batch in the current session has only finalized messages.
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"NO_ACTION\""))
      << fetcher->upload_data();
  EXPECT_EQ(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"PENDING\""))
      << fetcher->upload_data();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find(original_hash_string))
      << fetcher->upload_data();
  fetchers_.RemoveFetcherFromMap(kUrlFetcherId);

  // The next session starts on the next spellchecker request. Until then,
  // there's no more feedback sent.
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  EXPECT_EQ(NULL, GetFetcher());

  // The first spellcheck request after session expiration creates different
  // document marker hash identifiers.
  std::vector<SpellCheckMarker> original_markers(
      1, SpellCheckMarker(results[0].hash, results[0].location));
  results[0] = SpellCheckResult(SpellCheckResult::SPELLING,
                                kMisspellingStart,
                                kMisspellingLength,
                                ASCIIToUTF16("Hello"));
  feedback_->OnSpellcheckResults(
      &results, kRendererProcessId, kText, original_markers);
  uint32 updated_hash = results[0].hash;
  EXPECT_NE(updated_hash, original_hash);
  remaining_markers[0] = updated_hash;

  // The first feedback message batch in session |i + 1| has the new document
  // marker hash identifiers.
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  fetcher = GetFetcher();
  EXPECT_EQ(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"NO_ACTION\""))
      << fetcher->upload_data();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"actionType\":\"PENDING\""))
      << fetcher->upload_data();
  EXPECT_EQ(std::string::npos,
            fetcher->upload_data().find(original_hash_string))
      << fetcher->upload_data();
  std::string updated_hash_string =
      base::StringPrintf("\"suggestionId\":\"%u\"", updated_hash);
  EXPECT_NE(std::string::npos, fetcher->upload_data().find(updated_hash_string))
      << fetcher->upload_data();
}

// First message in session has an indicator.
TEST_F(FeedbackSenderTest, FirstMessageInSessionIndicator) {
  // Session 1, message 1
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"isFirstInSession\":true"))
      << fetcher->upload_data();

  // Session 1, message 2
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"isFirstInSession\":false"))
      << fetcher->upload_data();

  ExpireSession();

  // Session 1, message 3 (last)
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"isFirstInSession\":false"))
      << fetcher->upload_data();

  // Session 2, message 1
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"isFirstInSession\":true"))
      << fetcher->upload_data();

  // Session 2, message 2
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"isFirstInSession\":false"))
      << fetcher->upload_data();
}

// Flush all feedback when the spellcheck language and country change.
TEST_F(FeedbackSenderTest, OnLanguageCountryChange) {
  AddPendingFeedback();
  feedback_->OnLanguageCountryChange("pt", "BR");
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"language\":\"en\""))
      << fetcher->upload_data();

  AddPendingFeedback();
  feedback_->OnLanguageCountryChange("en", "US");
  fetcher = GetFetcher();
  EXPECT_NE(std::string::npos,
            fetcher->upload_data().find("\"language\":\"pt\""))
      << fetcher->upload_data();
}

// The field names and types should correspond to the API.
TEST_F(FeedbackSenderTest, FeedbackAPI) {
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  std::string actual_data = GetFetcher()->upload_data();
  scoped_ptr<base::DictionaryValue> actual(
      static_cast<base::DictionaryValue*>(base::JSONReader::Read(actual_data)));
  actual->SetString("params.key", "TestDummyKey");
  base::ListValue* suggestions = NULL;
  actual->GetList("params.suggestionInfo", &suggestions);
  base::DictionaryValue* suggestion = NULL;
  suggestions->GetDictionary(0, &suggestion);
  suggestion->SetString("suggestionId", "42");
  suggestion->SetString("timestamp", "9001");
  static const std::string expected_data =
      "{\"apiVersion\":\"v2\","
      "\"method\":\"spelling.feedback\","
      "\"params\":"
      "{\"clientName\":\"Chrome\","
      "\"originCountry\":\"USA\","
      "\"key\":\"TestDummyKey\","
      "\"language\":\"en\","
      "\"suggestionInfo\":[{"
      "\"isAutoCorrection\":false,"
      "\"isFirstInSession\":true,"
      "\"misspelledLength\":6,"
      "\"misspelledStart\":0,"
      "\"originalText\":\"Helllo world\","
      "\"suggestionId\":\"42\","
      "\"suggestions\":[\"Hello\"],"
      "\"timestamp\":\"9001\","
      "\"userActions\":[{\"actionType\":\"NO_ACTION\"}]}]}}";
  scoped_ptr<base::Value> expected(base::JSONReader::Read(expected_data));
  EXPECT_TRUE(expected->Equals(actual.get()))
      << "Expected data: " << expected_data
      << "\nActual data:   " << actual_data;
}

// Duplicate spellcheck results should be matched to the existing markers.
TEST_F(FeedbackSenderTest, MatchDupliateResultsWithExistingMarkers) {
  uint32 hash = AddPendingFeedback();
  std::vector<SpellCheckResult> results(
      1,
      SpellCheckResult(SpellCheckResult::SPELLING,
                       kMisspellingStart + 10,
                       kMisspellingLength,
                       ASCIIToUTF16("Hello")));
  std::vector<SpellCheckMarker> markers(
      1, SpellCheckMarker(hash, results[0].location));
  EXPECT_EQ(static_cast<uint32>(0), results[0].hash);
  feedback_->OnSpellcheckResults(&results, kRendererProcessId, kText, markers);
  EXPECT_EQ(hash, results[0].hash);
}

namespace {

// Returns the number of times that |needle| appears in |haystack| without
// overlaps. For example, CountOccurences("bananana", "nana") returns 1.
int CountOccurences(const std::string& haystack, const std::string& needle) {
  int number_of_occurences = 0;
  size_t pos = haystack.find(needle);
  while (pos != std::string::npos) {
    ++number_of_occurences;
    pos = haystack.find(needle, pos + needle.length());
  }
  return number_of_occurences;
}

}  // namespace

// Adding a word to dictionary should trigger ADD_TO_DICT feedback for every
// occurrence of that word.
TEST_F(FeedbackSenderTest, MultipleAddToDictFeedback) {
  std::vector<SpellCheckResult> results;
  static const int kSentenceLength = 14;
  static const int kNumberOfSentences = 2;
  static const string16 kTextWithDuplicates =
      ASCIIToUTF16("Helllo world. Helllo world.");
  for (int i = 0; i < kNumberOfSentences; ++i) {
    results.push_back(SpellCheckResult(SpellCheckResult::SPELLING,
                                       kMisspellingStart + i * kSentenceLength,
                                       kMisspellingLength,
                                       ASCIIToUTF16("Hello")));
  }
  static const int kNumberOfRenderers = 2;
  int last_renderer_process_id = -1;
  for (int i = 0; i < kNumberOfRenderers; ++i) {
    feedback_->OnSpellcheckResults(&results,
                                   kRendererProcessId + i,
                                   kTextWithDuplicates,
                                   std::vector<SpellCheckMarker>());
    last_renderer_process_id = kRendererProcessId + i;
  }
  std::vector<uint32> remaining_markers;
  for (size_t i = 0; i < results.size(); ++i)
    remaining_markers.push_back(results[i].hash);
  feedback_->OnReceiveDocumentMarkers(last_renderer_process_id,
                                      remaining_markers);
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_EQ(2, CountOccurences(fetcher->upload_data(), "PENDING"))
      << fetcher->upload_data();
  EXPECT_EQ(0, CountOccurences(fetcher->upload_data(), "ADD_TO_DICT"))
      << fetcher->upload_data();

  feedback_->AddedToDictionary(results[0].hash);
  feedback_->OnReceiveDocumentMarkers(last_renderer_process_id,
                                      remaining_markers);
  fetcher = GetFetcher();
  EXPECT_EQ(0, CountOccurences(fetcher->upload_data(), "PENDING"))
      << fetcher->upload_data();
  EXPECT_EQ(2, CountOccurences(fetcher->upload_data(), "ADD_TO_DICT"))
      << fetcher->upload_data();
}

// ADD_TO_DICT feedback for multiple occurrences of a word should trigger only
// for pending feedback.
TEST_F(FeedbackSenderTest, AddToDictOnlyPending) {
  AddPendingFeedback();
  uint32 add_to_dict_hash = AddPendingFeedback();
  uint32 select_hash = AddPendingFeedback();
  feedback_->SelectedSuggestion(select_hash, 0);
  feedback_->AddedToDictionary(add_to_dict_hash);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32>());
  net::TestURLFetcher* fetcher = GetFetcher();
  EXPECT_EQ(1, CountOccurences(fetcher->upload_data(), "SELECT"))
      << fetcher->upload_data();
  EXPECT_EQ(2, CountOccurences(fetcher->upload_data(), "ADD_TO_DICT"))
      << fetcher->upload_data();
}

}  // namespace spellcheck
