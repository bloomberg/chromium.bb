// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_metrics_reporter.h"

#include "base/metrics/histogram_macros.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_ukm_entry.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace contextual_suggestions {

ContextualSuggestionsMetricsReporterProvider::
    ContextualSuggestionsMetricsReporterProvider() = default;

ContextualSuggestionsMetricsReporterProvider::
    ~ContextualSuggestionsMetricsReporterProvider() = default;

std::unique_ptr<ContextualSuggestionsMetricsReporter>
ContextualSuggestionsMetricsReporterProvider::CreateMetricsReporter() {
  return std::make_unique<ContextualSuggestionsMetricsReporter>();
}

ContextualSuggestionsMetricsReporter::ContextualSuggestionsMetricsReporter()
    : sheet_peeked_(false),
      sheet_opened_(false),
      sheet_closed_(false),
      any_suggestion_downloaded_(false),
      any_suggestion_taken_(false),
      ukm_entry_(nullptr) {}

ContextualSuggestionsMetricsReporter::~ContextualSuggestionsMetricsReporter() {
  DCHECK(!ukm_entry_) << "Flush should be called before destruction!";
}

void ContextualSuggestionsMetricsReporter::SetupForPage(
    ukm::SourceId source_id) {
  DCHECK(!ukm_entry_) << "Flush should be called before SetupForPage!";
  DCHECK(source_id != ukm::kInvalidSourceId);
  ukm_entry_.reset(new ContextualSuggestionsUkmEntry(source_id));
}

void ContextualSuggestionsMetricsReporter::RecordEvent(
    ContextualSuggestionsEvent event) {
  DCHECK(ukm_entry_)
      << "Page not set up.  Did you forget to call SetupForPage or Flush?";
  ukm_entry_->RecordEventMetrics(event);
  RecordUmaMetrics(event);
}

void ContextualSuggestionsMetricsReporter::Flush() {
  ResetUma();
  if (ukm_entry_) {
    ukm_entry_->Flush();
    ukm_entry_.reset();
  }
}

void ContextualSuggestionsMetricsReporter::RecordUmaMetrics(
    ContextualSuggestionsEvent event) {
  switch (event) {
    case FETCH_DELAYED:
    case FETCH_REQUESTED:
    case FETCH_ERROR:
    case FETCH_SERVER_BUSY:
    case FETCH_BELOW_THRESHOLD:
    case FETCH_EMPTY:
    case FETCH_COMPLETED:
      break;
    case UI_PEEK_REVERSE_SCROLL:
      if (sheet_peeked_)
        return;
      sheet_peeked_ = true;
      break;
    case UI_OPENED:
      if (sheet_opened_)
        return;
      sheet_opened_ = true;
      break;
    case UI_CLOSED:
      if (sheet_closed_)
        return;
      sheet_closed_ = true;
      break;
    case SUGGESTION_DOWNLOADED:
      if (any_suggestion_downloaded_)
        return;
      any_suggestion_downloaded_ = true;
      break;
    case SUGGESTION_CLICKED:
      if (any_suggestion_taken_)
        return;
      any_suggestion_taken_ = true;
      break;
    default:
      NOTREACHED() << "Unexpected event, not correctly handled!";
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("ContextualSuggestions.Events", event);
}

void ContextualSuggestionsMetricsReporter::ResetUma() {
  sheet_peeked_ = false;
  sheet_opened_ = false;
  sheet_closed_ = false;
  any_suggestion_downloaded_ = false;
  any_suggestion_taken_ = false;
}

}  // namespace contextual_suggestions
