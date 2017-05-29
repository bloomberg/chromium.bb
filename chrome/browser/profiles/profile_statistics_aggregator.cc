// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics_aggregator.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "components/browsing_data/core/counters/bookmark_counter.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

using browsing_data::BrowsingDataCounter;

namespace {

// Number of statistic categories calculated by StartAggregator.
const int kProfileStatisticCategories = 4;

// Callback for each pref. Every one that should be counted as a changed
// user pref will cause *count to be incremented.
void AccumulatePrefStats(const PrefService* pref_service,
                         int* count,
                         const std::string& key,
                         const base::Value& value) {
  const PrefService::Preference* pref = pref_service->FindPreference(key);
  // Skip all dictionaries, only want to count values.
  if (!value.is_dict() && pref && pref->IsUserControlled() &&
      !pref->IsDefaultValue())
    ++(*count);
}

}  // namespace


void ProfileStatisticsAggregator::PasswordStoreConsumerHelper::
    OnGetPasswordStoreResults(
        std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  parent_->StatisticsCallbackSuccess(
      profiles::kProfileStatisticsPasswords, results.size());
}

ProfileStatisticsAggregator::ProfileStatisticsAggregator(
    Profile* profile,
    const base::Closure& done_callback)
    : profile_(profile),
      profile_path_(profile_->GetPath()),
      done_callback_(done_callback),
      password_store_consumer_helper_(this) {}

ProfileStatisticsAggregator::~ProfileStatisticsAggregator() {}

size_t ProfileStatisticsAggregator::GetCallbackCount() {
  return stats_callbacks_.size();
}

void ProfileStatisticsAggregator::AddCallbackAndStartAggregator(
    const profiles::ProfileStatisticsCallback& stats_callback) {
  if (stats_callback)
    stats_callbacks_.push_back(stats_callback);
  StartAggregator();
}

void ProfileStatisticsAggregator::AddCounter(
    std::unique_ptr<BrowsingDataCounter> counter) {
  counter->InitWithoutPref(
      base::Time(), base::Bind(&ProfileStatisticsAggregator::OnCounterResult,
                               base::Unretained(this)));
  counter->Restart();
  counters_.push_back(std::move(counter));
}

void ProfileStatisticsAggregator::StartAggregator() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(g_browser_process->profile_manager()->IsValidProfile(profile_));
  profile_category_stats_.clear();

  // Try to cancel tasks from task trackers.
  tracker_.TryCancelAll();
  counters_.clear();
  password_store_consumer_helper_.cancelable_task_tracker()->TryCancelAll();

  // Initiate bookmark counting.
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContextIfExists(profile_);
  if (bookmark_model) {
    AddCounter(
        base::MakeUnique<browsing_data::BookmarkCounter>(bookmark_model));
  } else {
    StatisticsCallbackFailure(profiles::kProfileStatisticsBookmarks);
  }

  // Initiate history counting (async).
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile_);

  if (history_service) {
    history_service->GetHistoryCount(
        base::Time(),
        base::Time::Max(),
        base::Bind(&ProfileStatisticsAggregator::StatisticsCallbackHistory,
                   this),
        &tracker_);
  } else {
    StatisticsCallbackFailure(profiles::kProfileStatisticsBrowsingHistory);
  }

  // Initiate stored password counting (async).
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (password_store) {
    password_store->GetAutofillableLogins(&password_store_consumer_helper_);
  } else {
    StatisticsCallbackFailure(profiles::kProfileStatisticsPasswords);
  }

  // Initiate preference counting (async).
  tracker_.PostTaskAndReplyWithResult(
      content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
          .get(),
      FROM_HERE, base::Bind(&ProfileStatisticsAggregator::CountPrefs, this),
      base::Bind(&ProfileStatisticsAggregator::StatisticsCallback, this,
                 profiles::kProfileStatisticsSettings));
}

void ProfileStatisticsAggregator::OnCounterResult(
    std::unique_ptr<BrowsingDataCounter::Result> result) {
  if (!result->Finished())
    return;
  const std::string& pref = result->source()->GetPrefName();
  auto* finished_result =
      static_cast<BrowsingDataCounter::FinishedResult*>(result.get());
  int count = finished_result->Value();
  if (pref == browsing_data::BookmarkCounter::kPrefName) {
    StatisticsCallbackSuccess(profiles::kProfileStatisticsBookmarks, count);
  } else {
    // TODO(dullweber): Add more cases when the other statistics are replaced.
    NOTREACHED();
  }
}

void ProfileStatisticsAggregator::StatisticsCallback(
    const char* category, ProfileStatValue result) {
  profiles::ProfileCategoryStat datum;
  datum.category = category;
  datum.count = result.count;
  datum.success = result.success;
  profile_category_stats_.push_back(datum);
  for (const auto& stats_callback : stats_callbacks_) {
    DCHECK(stats_callback);
    stats_callback.Run(profile_category_stats_);
  }

  if (result.success) {
    ProfileStatistics::SetProfileStatisticsToAttributesStorage(
        profile_path_, datum.category, result.count);
  }
  if (profile_category_stats_.size() == kProfileStatisticCategories) {
    if (done_callback_)
      done_callback_.Run();
  }
}

void ProfileStatisticsAggregator::StatisticsCallbackSuccess(
    const char* category, int count) {
  ProfileStatValue result;
  result.count = count;
  result.success = true;
  StatisticsCallback(category, result);
}

void ProfileStatisticsAggregator::StatisticsCallbackFailure(
    const char* category) {
  ProfileStatValue result;
  result.count = 0;
  result.success = false;
  StatisticsCallback(category, result);
}

void ProfileStatisticsAggregator::StatisticsCallbackHistory(
    history::HistoryCountResult result) {
  ProfileStatValue result_converted;
  result_converted.count = result.count;
  result_converted.success = result.success;
  StatisticsCallback(profiles::kProfileStatisticsBrowsingHistory,
                     result_converted);
}

ProfileStatisticsAggregator::ProfileStatValue
    ProfileStatisticsAggregator::CountPrefs() const {
  const PrefService* pref_service = profile_->GetPrefs();

  ProfileStatValue result;
  if (pref_service) {
    pref_service->IteratePreferenceValues(base::BindRepeating(
        &AccumulatePrefStats, pref_service, base::Unretained(&result.count)));
    result.success = true;
  }
  return result;
}
