// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The |FeedbackSender| object stores the user feedback to spellcheck
// suggestions in a |Feedback| object.
//
// When spelling service returns spellcheck results, these results first arrive
// in |FeedbackSender| to assign hash identifiers for each
// misspelling-suggestion pair. If the spelling service identifies the same
// misspelling as already displayed to the user, then |FeedbackSender| reuses
// the same hash identifiers to avoid duplication. It detects the duplicates by
// comparing misspelling offsets in text. Spelling service can return duplicates
// because we request spellcheck for whole paragraphs, as context around a
// misspelled word is important to the spellcheck algorithm.
//
// All feedback is initially pending. When a user acts upon a misspelling such
// that the misspelling is no longer displayed (red squiggly line goes away),
// then the feedback for this misspelling is finalized. All finalized feedback
// is erased after being sent to the spelling service. Pending feedback is kept
// around for |kSessionHours| hours and then finalized even if user did not act
// on the misspellings.
//
// |FeedbackSender| periodically requests a list of hashes of all remaining
// misspellings in renderers. When a renderer responds with a list of hashes,
// |FeedbackSender| uses the list to determine which misspellings are no longer
// displayed to the user and sends the current state of user feedback to the
// spelling service.

#include "chrome/browser/spellchecker/feedback_sender.h"

#include <algorithm>
#include <iterator>

#include "base/command_line.h"
#include "base/hash.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/spellchecker/word_trimmer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_marker.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/render_process_host.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace spellcheck {

namespace {

// The default URL where feedback data is sent.
const char kFeedbackServiceURL[] = "https://www.googleapis.com/rpc";

// The minimum number of seconds between sending batches of feedback.
const int kMinIntervalSeconds = 5;

// Returns a hash of |session_start|, the current timestamp, and
// |suggestion_index|.
uint32 BuildHash(const base::Time& session_start, size_t suggestion_index) {
  return base::Hash(
      base::StringPrintf("%" PRId64 "%" PRId64 "%" PRIuS,
                         session_start.ToInternalValue(),
                         base::Time::Now().ToInternalValue(),
                         suggestion_index));
}

// Returns a pending feedback data structure for the spellcheck |result| and
// |text|.
Misspelling BuildFeedback(const SpellCheckResult& result,
                          const base::string16& text) {
  size_t start = result.location;
  base::string16 context = TrimWords(&start,
                               result.length,
                               text,
                               chrome::spellcheck_common::kContextWordCount);
  return Misspelling(context,
                     start,
                     result.length,
                     std::vector<base::string16>(1, result.replacement),
                     result.hash);
}

// Builds suggestion info from |suggestions|. The caller owns the result.
base::ListValue* BuildSuggestionInfo(
    const std::vector<Misspelling>& suggestions,
    bool is_first_feedback_batch) {
  base::ListValue* list = new base::ListValue;
  for (std::vector<Misspelling>::const_iterator suggestion_it =
           suggestions.begin();
       suggestion_it != suggestions.end();
       ++suggestion_it) {
    base::DictionaryValue* suggestion = suggestion_it->Serialize();
    suggestion->SetBoolean("isFirstInSession", is_first_feedback_batch);
    suggestion->SetBoolean("isAutoCorrection", false);
    list->Append(suggestion);
  }
  return list;
}

// Builds feedback parameters from |suggestion_info|, |language|, and |country|.
// Takes ownership of |suggestion_list|. The caller owns the result.
base::DictionaryValue* BuildParams(base::ListValue* suggestion_info,
                                   const std::string& language,
                                   const std::string& country) {
  base::DictionaryValue* params = new base::DictionaryValue;
  params->Set("suggestionInfo", suggestion_info);
  params->SetString("key", google_apis::GetAPIKey());
  params->SetString("language", language);
  params->SetString("originCountry", country);
  params->SetString("clientName", "Chrome");
  return params;
}

// Builds feedback data from |params|. Takes ownership of |params|. The caller
// owns the result.
base::Value* BuildFeedbackValue(base::DictionaryValue* params,
                                const std::string& api_version) {
  base::DictionaryValue* result = new base::DictionaryValue;
  result->Set("params", params);
  result->SetString("method", "spelling.feedback");
  result->SetString("apiVersion", api_version);
  return result;
}

// Returns true if the misspelling location is within text bounds.
bool IsInBounds(int misspelling_location,
                int misspelling_length,
                size_t text_length) {
  return misspelling_location >= 0 && misspelling_length > 0 &&
         static_cast<size_t>(misspelling_location) < text_length &&
         static_cast<size_t>(misspelling_location + misspelling_length) <=
             text_length;
}

// Returns the feedback API version.
std::string GetApiVersion() {
  // This guard is temporary.
  // TODO(rouslan): Remove the guard. http://crbug.com/247726
  if (base::FieldTrialList::FindFullName(kFeedbackFieldTrialName) ==
          kFeedbackFieldTrialEnabledGroupName &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSpellingFeedbackFieldTrial)) {
    return "v2-internal";
  }
  return "v2";
}

}  // namespace

FeedbackSender::FeedbackSender(net::URLRequestContextGetter* request_context,
                               const std::string& language,
                               const std::string& country)
    : request_context_(request_context),
      api_version_(GetApiVersion()),
      language_(language),
      country_(country),
      misspelling_counter_(0),
      session_start_(base::Time::Now()),
      feedback_service_url_(kFeedbackServiceURL) {
  // The command-line switch is for testing and temporary.
  // TODO(rouslan): Remove the command-line switch when testing is complete.
  // http://crbug.com/247726
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSpellingServiceFeedbackUrl)) {
    feedback_service_url_ =
        GURL(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kSpellingServiceFeedbackUrl));
  }
}

FeedbackSender::~FeedbackSender() {
}

void FeedbackSender::SelectedSuggestion(uint32 hash, int suggestion_index) {
  Misspelling* misspelling = feedback_.GetMisspelling(hash);
  // GetMisspelling() returns null for flushed feedback. Feedback is flushed
  // when the session expires every |kSessionHours| hours.
  if (!misspelling)
    return;
  misspelling->action.type = SpellcheckAction::TYPE_SELECT;
  misspelling->action.index = suggestion_index;
  misspelling->timestamp = base::Time::Now();
}

void FeedbackSender::AddedToDictionary(uint32 hash) {
  Misspelling* misspelling = feedback_.GetMisspelling(hash);
  // GetMisspelling() returns null for flushed feedback. Feedback is flushed
  // when the session expires every |kSessionHours| hours.
  if (!misspelling)
    return;
  misspelling->action.type = SpellcheckAction::TYPE_ADD_TO_DICT;
  misspelling->timestamp = base::Time::Now();
  const std::set<uint32>& hashes =
      feedback_.FindMisspellings(misspelling->GetMisspelledString());
  for (std::set<uint32>::const_iterator hash_it = hashes.begin();
       hash_it != hashes.end();
       ++hash_it) {
    Misspelling* duplicate_misspelling = feedback_.GetMisspelling(*hash_it);
    if (!duplicate_misspelling || duplicate_misspelling->action.IsFinal())
      continue;
    duplicate_misspelling->action.type = SpellcheckAction::TYPE_ADD_TO_DICT;
    duplicate_misspelling->timestamp = misspelling->timestamp;
  }
}

void FeedbackSender::RecordInDictionary(uint32 hash) {
  Misspelling* misspelling = feedback_.GetMisspelling(hash);
  // GetMisspelling() returns null for flushed feedback. Feedback is flushed
  // when the session expires every |kSessionHours| hours.
  if (!misspelling)
    return;
  misspelling->action.type = SpellcheckAction::TYPE_IN_DICTIONARY;
}

void FeedbackSender::IgnoredSuggestions(uint32 hash) {
  Misspelling* misspelling = feedback_.GetMisspelling(hash);
  // GetMisspelling() returns null for flushed feedback. Feedback is flushed
  // when the session expires every |kSessionHours| hours.
  if (!misspelling)
    return;
  misspelling->action.type = SpellcheckAction::TYPE_PENDING_IGNORE;
  misspelling->timestamp = base::Time::Now();
}

void FeedbackSender::ManuallyCorrected(uint32 hash,
                                       const base::string16& correction) {
  Misspelling* misspelling = feedback_.GetMisspelling(hash);
  // GetMisspelling() returns null for flushed feedback. Feedback is flushed
  // when the session expires every |kSessionHours| hours.
  if (!misspelling)
    return;
  misspelling->action.type = SpellcheckAction::TYPE_MANUALLY_CORRECTED;
  misspelling->action.value = correction;
  misspelling->timestamp = base::Time::Now();
}

void FeedbackSender::OnReceiveDocumentMarkers(
    int renderer_process_id,
    const std::vector<uint32>& markers) {
  if ((base::Time::Now() - session_start_).InHours() >=
      chrome::spellcheck_common::kSessionHours) {
    FlushFeedback();
    return;
  }

  if (!feedback_.RendererHasMisspellings(renderer_process_id))
    return;

  feedback_.FinalizeRemovedMisspellings(renderer_process_id, markers);
  SendFeedback(feedback_.GetMisspellingsInRenderer(renderer_process_id),
               !renderers_sent_feedback_.count(renderer_process_id));
  renderers_sent_feedback_.insert(renderer_process_id);
  feedback_.EraseFinalizedMisspellings(renderer_process_id);
}

void FeedbackSender::OnSpellcheckResults(
    int renderer_process_id,
    const base::string16& text,
    const std::vector<SpellCheckMarker>& markers,
    std::vector<SpellCheckResult>* results) {
  // Don't collect feedback if not going to send it.
  if (!timer_.IsRunning())
    return;

  // Generate a map of marker offsets to marker hashes. This map helps to
  // efficiently lookup feedback data based on the position of the misspelling
  // in text.
  typedef std::map<size_t, uint32> MarkerMap;
  MarkerMap marker_map;
  for (size_t i = 0; i < markers.size(); ++i)
    marker_map[markers[i].offset] = markers[i].hash;

  for (std::vector<SpellCheckResult>::iterator result_it = results->begin();
       result_it != results->end();
       ++result_it) {
    if (!IsInBounds(result_it->location, result_it->length, text.length()))
      continue;
    MarkerMap::const_iterator marker_it = marker_map.find(result_it->location);
    if (marker_it != marker_map.end() &&
        feedback_.HasMisspelling(marker_it->second)) {
      // If the renderer already has a marker for this spellcheck result, then
      // set the hash of the spellcheck result to be the same as the marker.
      result_it->hash = marker_it->second;
    } else {
      // If the renderer does not yet have a marker for this spellcheck result,
      // then generate a new hash for the spellcheck result.
      result_it->hash = BuildHash(session_start_, ++misspelling_counter_);
    }
    // Save the feedback data for the spellcheck result.
    feedback_.AddMisspelling(renderer_process_id,
                             BuildFeedback(*result_it, text));
  }
}

void FeedbackSender::OnLanguageCountryChange(const std::string& language,
                                             const std::string& country) {
  FlushFeedback();
  language_ = language;
  country_ = country;
}

void FeedbackSender::StartFeedbackCollection() {
  if (timer_.IsRunning())
    return;

  int interval_seconds = chrome::spellcheck_common::kFeedbackIntervalSeconds;
  // This command-line switch is for testing and temporary.
  // TODO(rouslan): Remove the command-line switch when testing is complete.
  // http://crbug.com/247726
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSpellingServiceFeedbackIntervalSeconds)) {
    base::StringToInt(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                          switches::kSpellingServiceFeedbackIntervalSeconds),
                      &interval_seconds);
    if (interval_seconds < kMinIntervalSeconds)
      interval_seconds = kMinIntervalSeconds;
    static const int kSessionSeconds =
        chrome::spellcheck_common::kSessionHours * 60 * 60;
    if (interval_seconds >  kSessionSeconds)
      interval_seconds = kSessionSeconds;
  }
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(interval_seconds),
               this,
               &FeedbackSender::RequestDocumentMarkers);
}

void FeedbackSender::StopFeedbackCollection() {
  if (!timer_.IsRunning())
    return;

  FlushFeedback();
  timer_.Stop();
}

void FeedbackSender::OnURLFetchComplete(const net::URLFetcher* source) {
  for (ScopedVector<net::URLFetcher>::iterator sender_it = senders_.begin();
       sender_it != senders_.end();
       ++sender_it) {
    if (*sender_it == source) {
      senders_.erase(sender_it);
      return;
    }
  }
  delete source;
}

void FeedbackSender::RequestDocumentMarkers() {
  // Request document markers from all the renderers that are still alive.
  std::set<int> alive_renderers;
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd();
       it.Advance()) {
    alive_renderers.insert(it.GetCurrentValue()->GetID());
    it.GetCurrentValue()->Send(new SpellCheckMsg_RequestDocumentMarkers());
  }

  // Asynchronously send out the feedback for all the renderers that are no
  // longer alive.
  std::vector<int> known_renderers = feedback_.GetRendersWithMisspellings();
  std::sort(known_renderers.begin(), known_renderers.end());
  std::vector<int> dead_renderers =
      base::STLSetDifference<std::vector<int> >(known_renderers,
                                                alive_renderers);
  for (std::vector<int>::const_iterator it = dead_renderers.begin();
       it != dead_renderers.end();
       ++it) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FeedbackSender::OnReceiveDocumentMarkers,
                   AsWeakPtr(),
                   *it,
                   std::vector<uint32>()));
  }
}

void FeedbackSender::FlushFeedback() {
  if (feedback_.Empty())
    return;
  feedback_.FinalizeAllMisspellings();
  SendFeedback(feedback_.GetAllMisspellings(),
               renderers_sent_feedback_.empty());
  feedback_.Clear();
  renderers_sent_feedback_.clear();
  session_start_ = base::Time::Now();
  timer_.Reset();
}

void FeedbackSender::SendFeedback(const std::vector<Misspelling>& feedback_data,
                                  bool is_first_feedback_batch) {
  scoped_ptr<base::Value> feedback_value(BuildFeedbackValue(
      BuildParams(BuildSuggestionInfo(feedback_data, is_first_feedback_batch),
                  language_,
                  country_),
      api_version_));
  std::string feedback;
  base::JSONWriter::Write(feedback_value.get(), &feedback);

  // The tests use this identifier to mock the URL fetcher.
  static const int kUrlFetcherId = 0;
  net::URLFetcher* sender = net::URLFetcher::Create(
      kUrlFetcherId, feedback_service_url_, net::URLFetcher::POST, this);
  sender->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                       net::LOAD_DO_NOT_SAVE_COOKIES);
  sender->SetUploadData("application/json", feedback);
  senders_.push_back(sender);

  // Request context is NULL in testing.
  if (request_context_.get()) {
    sender->SetRequestContext(request_context_.get());
    sender->Start();
  }
}

}  // namespace spellcheck
