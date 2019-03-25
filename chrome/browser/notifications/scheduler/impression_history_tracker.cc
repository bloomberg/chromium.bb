// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/impression_history_tracker.h"

#include <utility>

#include "base/numerics/ranges.h"
#include "base/optional.h"

namespace notifications {

ImpressionHistoryTrackerImpl::ImpressionHistoryTrackerImpl(
    const SchedulerConfig& config,
    TypeStates type_states)
    : type_states_(std::move(type_states)), config_(config) {}

ImpressionHistoryTrackerImpl::~ImpressionHistoryTrackerImpl() = default;

void ImpressionHistoryTrackerImpl::AnalyzeImpressionHistory() {
  for (auto& type_state : type_states_)
    AnalyzeImpressionHistory(type_state.second.get());
}

const ImpressionHistoryTracker::TypeStates&
ImpressionHistoryTrackerImpl::GetTypeStates() const {
  return type_states_;
}

void ImpressionHistoryTrackerImpl::AnalyzeImpressionHistory(
    TypeState* type_state) {
  DCHECK(type_state);
  std::deque<Impression*> dismisses;
  base::Time now = base::Time::Now();

  for (auto it = type_state->impressions.begin();
       it != type_state->impressions.end();) {
    auto* impression = &it->second;

    // Prune out expired impression.
    if (now - impression->create_time > config_.impression_expiration) {
      type_state->impressions.erase(it++);
      continue;
    } else {
      ++it;
    }

    // TODO(xingliu): Use a delegate to get ImpressionResult from notification
    // scheduler API consumer.
    switch (impression->feedback) {
      case UserFeedback::kNotHelpful:
        // One unhelpful clicks results in suppression.
        ApplyNegativeImpression(type_state, impression);
        break;
      case UserFeedback::kDismiss:
        dismisses.emplace_back(impression);
        PruneImpression(&dismisses,
                        impression->create_time - config_.dismiss_duration);

        // Three consecutive dismisses will result in suppression.
        ApplyNegativeImpressions(type_state, &dismisses, config_.dismiss_count);
        break;
      case UserFeedback::kClick:
      case UserFeedback::kHelpful:
        // Any body click or helpful button click will increase maximum
        // notification deliver.
        ApplyPositiveImpression(type_state, impression);
        break;
      case UserFeedback::kIgnore:
        break;
      case UserFeedback::kUnknown:
        FALLTHROUGH;
      default:
        // The user didn't interact with the notification yet.
        continue;
        break;
    }
  }

  // Perform suppression recovery.
  SuppressionRecovery(type_state);
}

// static
void ImpressionHistoryTrackerImpl::PruneImpression(
    std::deque<Impression*>* impressions,
    const base::Time& start_time) {
  DCHECK(impressions);
  while (!impressions->empty()) {
    if (impressions->front()->create_time > start_time)
      break;
    // Anything created before |start_time| is considered to have no effect and
    // will never be processed again.
    impressions->front()->integrated = true;
    impressions->pop_front();
  }
}

void ImpressionHistoryTrackerImpl::ApplyPositiveImpression(
    TypeState* type_state,
    Impression* impression) {
  DCHECK(impression);
  if (impression->integrated)
    return;

  impression->integrated = true;
  impression->impression = ImpressionResult::kPositive;

  // Increase |current_max_daily_show| by 1.
  type_state->current_max_daily_show =
      base::ClampToRange(++type_state->current_max_daily_show, 0,
                         config_.max_daily_shown_per_type);
}

void ImpressionHistoryTrackerImpl::ApplyNegativeImpressions(
    TypeState* type_state,
    std::deque<Impression*>* impressions,
    size_t num_actions) {
  if (impressions->size() < num_actions)
    return;

  // Suppress the notification if the user performed consecutive operations that
  // generates negative impressions.
  for (size_t i = 0, size = impressions->size(); i < size; ++i) {
    if ((*impressions)[i]->integrated)
      continue;

    (*impressions)[i]->integrated = true;

    // Each user feedback after |num_action| will apply a new negative
    // impression.
    if (i + 1 >= num_actions)
      ApplyNegativeImpression(type_state, (*impressions)[i]);
  }
}

void ImpressionHistoryTrackerImpl::ApplyNegativeImpression(
    TypeState* type_state,
    Impression* impression) {
  if (impression->integrated)
    return;

  impression->integrated = true;
  impression->impression = ImpressionResult::kNegative;

  // Suppress the notification, the user will not see this type of notification
  // for a while.
  SuppressionInfo supression_info(base::Time::Now(),
                                  config_.suppression_duration);
  type_state->suppression_info = std::move(supression_info);
  type_state->current_max_daily_show = 0;
}

// Recovers from suppression caused by negative impressions.
void ImpressionHistoryTrackerImpl::SuppressionRecovery(TypeState* type_state) {
  // No suppression to recover from.
  if (!type_state->suppression_info.has_value())
    return;

  SuppressionInfo& suppression = type_state->suppression_info.value();
  base::Time now = base::Time::Now();

  // Still in the suppression time window.
  if (now - suppression.last_trigger_time < suppression.duration)
    return;

  // Recover from suppression and increase |current_max_daily_show|.
  DCHECK_EQ(type_state->current_max_daily_show, 0);
  type_state->current_max_daily_show = suppression.recover_goal;

  // Clear suppression if fully recovered.
  type_state->suppression_info.reset();
}

}  // namespace notifications
