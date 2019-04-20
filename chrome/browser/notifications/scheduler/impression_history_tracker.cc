// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/impression_history_tracker.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/numerics/ranges.h"

namespace notifications {

// Comparator used to sort notification entries based on creation time.
bool CreateTimeCompare(const Impression& lhs, const Impression& rhs) {
  return lhs.create_time < rhs.create_time;
}

ImpressionHistoryTrackerImpl::ImpressionHistoryTrackerImpl(
    const SchedulerConfig& config,
    std::unique_ptr<CollectionStore<ClientState>> store)
    : store_(std::move(store)),
      config_(config),
      initialized_(false),
      weak_ptr_factory_(this) {}

ImpressionHistoryTrackerImpl::~ImpressionHistoryTrackerImpl() = default;

void ImpressionHistoryTrackerImpl::Init(InitCallback callback) {
  store_->InitAndLoad(
      base::BindOnce(&ImpressionHistoryTrackerImpl::OnStoreInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ImpressionHistoryTrackerImpl::AnalyzeImpressionHistory() {
  for (auto& client_state : client_states_)
    AnalyzeImpressionHistory(client_state.second.get());
}

const ImpressionHistoryTracker::ClientStates&
ImpressionHistoryTrackerImpl::GetClientStates() const {
  return client_states_;
}

void ImpressionHistoryTrackerImpl::OnStoreInitialized(
    InitCallback callback,
    bool success,
    CollectionStore<ClientState>::Entries entries) {
  if (!success) {
    std::move(callback).Run(false);
    return;
  }

  initialized_ = true;

  // Load the data to memory, and sort the impression list.
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    auto& entry = (*it);
    auto type = entry->type;
    std::sort(entry->impressions.begin(), entry->impressions.end(),
              &CreateTimeCompare);
    client_states_.emplace(type, std::move(*it));
  }

  std::move(callback).Run(true);
}

void ImpressionHistoryTrackerImpl::AnalyzeImpressionHistory(
    ClientState* client_state) {
  DCHECK(client_state);
  std::deque<Impression*> dismisses;
  base::Time now = base::Time::Now();

  for (auto it = client_state->impressions.begin();
       it != client_state->impressions.end();) {
    auto* impression = &*it;

    // Prune out expired impression.
    if (now - impression->create_time > config_.impression_expiration) {
      client_state->impressions.erase(it++);
      continue;
    } else {
      ++it;
    }

    // TODO(xingliu): Use a delegate to get ImpressionResult from notification
    // scheduler API consumer.
    switch (impression->feedback) {
      case UserFeedback::kNotHelpful:
        // One unhelpful clicks results in suppression.
        ApplyNegativeImpression(client_state, impression);
        break;
      case UserFeedback::kDismiss:
        dismisses.emplace_back(impression);
        PruneImpression(&dismisses,
                        impression->create_time - config_.dismiss_duration);

        // Three consecutive dismisses will result in suppression.
        ApplyNegativeImpressions(client_state, &dismisses,
                                 config_.dismiss_count);
        break;
      case UserFeedback::kClick:
      case UserFeedback::kHelpful:
        // Any body click or helpful button click will increase maximum
        // notification deliver.
        ApplyPositiveImpression(client_state, impression);
        break;
      case UserFeedback::kIgnore:
        break;
      case UserFeedback::kNoFeedback:
        FALLTHROUGH;
      default:
        // The user didn't interact with the notification yet.
        continue;
        break;
    }
  }

  // Perform suppression recovery.
  SuppressionRecovery(client_state);
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
    ClientState* client_state,
    Impression* impression) {
  DCHECK(impression);
  if (impression->integrated)
    return;

  impression->integrated = true;
  impression->impression = ImpressionResult::kPositive;

  // Increase |current_max_daily_show| by 1.
  client_state->current_max_daily_show =
      base::ClampToRange(++client_state->current_max_daily_show, 0,
                         config_.max_daily_shown_per_type);
}

void ImpressionHistoryTrackerImpl::ApplyNegativeImpressions(
    ClientState* client_state,
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
      ApplyNegativeImpression(client_state, (*impressions)[i]);
  }
}

void ImpressionHistoryTrackerImpl::ApplyNegativeImpression(
    ClientState* client_state,
    Impression* impression) {
  if (impression->integrated)
    return;

  impression->integrated = true;
  impression->impression = ImpressionResult::kNegative;

  // Suppress the notification, the user will not see this type of notification
  // for a while.
  SuppressionInfo supression_info(base::Time::Now(),
                                  config_.suppression_duration);
  client_state->suppression_info = std::move(supression_info);
  client_state->current_max_daily_show = 0;
}

// Recovers from suppression caused by negative impressions.
void ImpressionHistoryTrackerImpl::SuppressionRecovery(
    ClientState* client_state) {
  // No suppression to recover from.
  if (!client_state->suppression_info.has_value())
    return;

  SuppressionInfo& suppression = client_state->suppression_info.value();
  base::Time now = base::Time::Now();

  // Still in the suppression time window.
  if (now - suppression.last_trigger_time < suppression.duration)
    return;

  // Recover from suppression and increase |current_max_daily_show|.
  DCHECK_EQ(client_state->current_max_daily_show, 0);
  client_state->current_max_daily_show = suppression.recover_goal;

  // Clear suppression if fully recovered.
  client_state->suppression_info.reset();
}

}  // namespace notifications
