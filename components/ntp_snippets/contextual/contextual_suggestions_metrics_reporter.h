// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_METRICS_REPORTER_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_METRICS_REPORTER_H_

#include <memory>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace contextual_suggestions {

class ContextualSuggestionsUkmEntry;

FORWARD_DECLARE_TEST(ContextualSuggestionsMetricsReporterTest, BaseTest);

// NOTE: because this is used for UMA reporting, these values should not be
// changed or reused; new values should be appended immediately before the
// EVENT_MAX value. Make sure to update the histogram enum
// (ContextualSuggestions.Event in enums.xml) accordingly!
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.contextual_suggestions)
enum ContextualSuggestionsEvent {
  // Indicates that this state is not initialized.
  // Should never be intentionally recorded, just used as a default value.
  UNINITIALIZED = 0,
  // Records that fetching suggestions has been delayed on the client side.
  FETCH_DELAYED = 1,
  // The fetch request has been made but a response has not yet been received.
  FETCH_REQUESTED = 2,
  // The fetch response has been received, but there was some error.
  FETCH_ERROR = 3,
  // The fetch response indicates that the server was too busy to return any
  // suggestions.
  FETCH_SERVER_BUSY = 4,
  // The fetch response includes suggestions but they were all below the
  // confidence threshold needed to show them to the user.
  FETCH_BELOW_THRESHOLD = 5,
  // The fetch response has been received and parsed, but there were no
  // suggestions.
  FETCH_EMPTY = 6,
  // The fetch response has been received and there are suggestions to show.
  FETCH_COMPLETED = 7,
  // The UI was shown in the "peeking bar" state, triggered by a reverse-scroll.
  // If new gestures are added to trigger the peeking sheet then a new event
  // should be added to this list.
  UI_PEEK_REVERSE_SCROLL = 8,
  // The UI sheet was opened.
  UI_OPENED = 9,
  // The UI was closed (via the close box).
  UI_CLOSED = 10,
  // A suggestion was downloaded.
  SUGGESTION_DOWNLOADED = 11,
  // A suggestion was taken, either with a click, or opened in a separate tab.
  SUGGESTION_CLICKED = 12,
  // Special name that marks the maximum value in an Enum used for UMA.
  // https://cs.chromium.org/chromium/src/tools/metrics/histograms/README.md.
  kMaxValue = SUGGESTION_CLICKED,
};

class ContextualSuggestionsMetricsReporter;

// Class producing |ContextualSuggestionsMetricsReporter| instances. It enables
// classes like |ContextualContentSuggestionService| to produce metrics
// reporters when needed, e.g. creation of service proxy, without knowing how to
// initialize them.
class ContextualSuggestionsMetricsReporterProvider {
 public:
  ContextualSuggestionsMetricsReporterProvider();
  virtual ~ContextualSuggestionsMetricsReporterProvider();

  virtual std::unique_ptr<ContextualSuggestionsMetricsReporter>
  CreateMetricsReporter();

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextualSuggestionsMetricsReporterProvider);
};

// Tracks various UKM and UMA metrics based on reports of events that take place
// within the Contextual Suggestions component.
//
// For example:
// Java UI -> ContextualSuggestionsBridge#reportEvent(
//     /* web_contents */, @ContextualSuggestionsEvent int eventId) -> native
//    -> ContextualContentSuggestionsService#reportEvent(
//          ukm::GetSourceIdForWebContentsDocument(web_contents), eventId)) ->
// if(!reporter) {
//   ContextualSuggestionsMetricsReporter* reporter =
//     new ContextualSuggestionsMetricsReporter();
// }
// ukm::SourceId source_id_for_web_contents;
// reporter->SetupForPage(source_id_for_web_contents);
// reporter->RecordEvent(FETCH_REQUESTED);
// ...
//
// if (!my_results)
//   reporter->RecordEvent(FETCH_ERROR);
// else if (my_result.size() == 0)
//   reporter->RecordEvent(FETCH_EMPTY);
// else
//   reporter->RecordEvent(FETCH_COMPLETED);
// ...
// When the UI shows:
// reporter->RecordEvent(UI_PEEK_SHOWN);
// Make sure data is flushed when leaving the page:
// reporter->Flush();
class ContextualSuggestionsMetricsReporter {
 public:
  ContextualSuggestionsMetricsReporter();
  ~ContextualSuggestionsMetricsReporter();

  // Sets up the page with the given |source_id| for event reporting.
  // All subsequent RecordEvent calls will apply to this page.
  void SetupForPage(ukm::SourceId source_id);

  // Reports that an event occurred for the current page.
  // Some data is not written immediately, but will be written when |Reset| is
  // called.
  void RecordEvent(ContextualSuggestionsEvent event);

  // Flushes all data staged using |RecordEvent|.
  // This is required before a subsequent call to |SetupForPage|, and can be
  // called multiple times.
  void Flush();

 private:
  FRIEND_TEST_ALL_PREFIXES(ContextualSuggestionsMetricsReporterTest, BaseTest);

  // Records UMA metrics for this event.
  void RecordUmaMetrics(ContextualSuggestionsEvent event);

  // Reset UMA metrics.
  void ResetUma();

  // Internal UMA state data.
  // Whether the sheet ever peeked.
  bool sheet_peeked_;
  // Whether the sheet was ever opened.
  bool sheet_opened_;
  // Whether the sheet was closed.
  bool sheet_closed_;
  // Whether any suggestion was downloaded.
  bool any_suggestion_downloaded_;
  // Whether any suggestion was taken.
  bool any_suggestion_taken_;

  // The current UKM entry.
  std::unique_ptr<ContextualSuggestionsUkmEntry> ukm_entry_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSuggestionsMetricsReporter);
};

using ReportFetchMetricsCallback = base::RepeatingCallback<void(
    contextual_suggestions::ContextualSuggestionsEvent event)>;

}  // namespace contextual_suggestions

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_METRICS_REPORTER_H_
