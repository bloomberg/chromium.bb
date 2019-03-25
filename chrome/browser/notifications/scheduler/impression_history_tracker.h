// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_IMPRESSION_HISTORY_TRACKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_IMPRESSION_HISTORY_TRACKER_H_

#include <deque>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/impression_types.h"
#include "chrome/browser/notifications/scheduler/scheduler_config.h"

namespace notifications {

// Provides functionalities to update notification impression history and adjust
// maximum daily notification shown to the user.
class ImpressionHistoryTracker {
 public:
  using TypeStates = std::map<SchedulerClientType, std::unique_ptr<TypeState>>;

  // Analyzes the impression history for all notification types, and adjusts
  // the |current_max_daily_show| in TypeState.
  virtual void AnalyzeImpressionHistory() = 0;

  // Queries the type states.
  virtual const TypeStates& GetTypeStates() const = 0;

  virtual ~ImpressionHistoryTracker() = default;

 protected:
  ImpressionHistoryTracker() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTracker);
};

// An implementation of ImpressionHistoryTracker backed by a database.
// TODO(xingliu): Pass in the store interface and retrieve |type_states_| from
// the database.
class ImpressionHistoryTrackerImpl : public ImpressionHistoryTracker {
 public:
  explicit ImpressionHistoryTrackerImpl(const SchedulerConfig& config,
                                        TypeStates type_states);
  ~ImpressionHistoryTrackerImpl() override;

 private:
  // ImpressionHistoryTracker implementation.
  void AnalyzeImpressionHistory() override;
  const ImpressionHistoryTracker::TypeStates& GetTypeStates() const override;

  // Helper method to prune impressions created before |start_time|. Assumes
  // |impressions| are sorted by creation time.
  static void PruneImpression(std::deque<Impression*>* impressions,
                              const base::Time& start_time);

  // Analyzes the impression history for |type|.
  void AnalyzeImpressionHistory(TypeState* type_state);

  // Applies a positive impression result to this notification type.
  void ApplyPositiveImpression(TypeState* type_state, Impression* impression);

  // Applies negative impression on this notification type when |num_actions|
  // consecutive negative impression result are generated.
  void ApplyNegativeImpressions(TypeState* type_state,
                                std::deque<Impression*>* impressions,
                                size_t num_actions);

  // Applies one negative impression.
  void ApplyNegativeImpression(TypeState* type_state, Impression* impression);

  // Recovers from suppression caused by negative impressions.
  void SuppressionRecovery(TypeState* type_state);

  // Impression history and global states for all notification types.
  TypeStates type_states_;

  // System configuration.
  const SchedulerConfig& config_;

  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTrackerImpl);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_IMPRESSION_HISTORY_TRACKER_H_
