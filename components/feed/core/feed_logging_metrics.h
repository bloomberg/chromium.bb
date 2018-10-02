// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_LOGGING_METRICS_H_
#define COMPONENTS_FEED_CORE_FEED_LOGGING_METRICS_H_

namespace base {
class Time;
class TimeDelta;
}  // namespace base

enum class WindowOpenDisposition;

namespace feed {
namespace metrics {

// |suggestions_count| contains how many cards show to users. It does not depend
// on whether the user actually saw the cards.
void OnPageShown(const int suggestions_count);

// Should only be called once per NTP for each suggestion.
void OnSuggestionShown(int position,
                       base::Time publish_date,
                       float score,
                       base::Time fetch_date);

void OnSuggestionOpened(int position,
                        base::Time publish_date,
                        float score,
                        WindowOpenDisposition disposition);

void OnSuggestionMenuOpened(int position, base::Time publish_date, float score);

void OnSuggestionDismissed(int position, bool visited);

void OnSuggestionArticleVisited(base::TimeDelta visit_time);

// Should only be called once per NTP for each "more" button.
void OnMoreButtonShown(int position);

void OnMoreButtonClicked(int position);

}  // namespace metrics
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_LOGGING_METRICS_H_
