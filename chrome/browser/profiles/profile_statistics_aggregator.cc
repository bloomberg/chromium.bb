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
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

using browsing_data::BrowsingDataCounter;

namespace {

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

ProfileStatisticsAggregator::ProfileStatisticsAggregator(
    Profile* profile,
    const base::Closure& done_callback)
    : profile_(profile),
      profile_path_(profile_->GetPath()),
      done_callback_(done_callback) {}

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

  // Try to cancel tasks.
  tracker_.TryCancelAll();
  counters_.clear();
  // Initiate bookmark counting.
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  AddCounter(base::MakeUnique<browsing_data::BookmarkCounter>(bookmark_model));

  // Initiate history counting.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  AddCounter(base::MakeUnique<browsing_data::HistoryCounter>(
      history_service,
      browsing_data::HistoryCounter::GetUpdatedWebHistoryServiceCallback(),
      /*sync_service=*/nullptr));

  // Initiate stored password counting.
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  AddCounter(base::MakeUnique<browsing_data::PasswordsCounter>(
      password_store, /*sync_service=*/nullptr));

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
  const char* pref_name = result->source()->GetPrefName();
  auto* finished_result =
      static_cast<BrowsingDataCounter::FinishedResult*>(result.get());
  int count = finished_result->Value();
  if (pref_name == browsing_data::BookmarkCounter::kPrefName) {
    StatisticsCallbackSuccess(profiles::kProfileStatisticsBookmarks, count);
  } else if (pref_name == browsing_data::prefs::kDeleteBrowsingHistory) {
    StatisticsCallbackSuccess(profiles::kProfileStatisticsBrowsingHistory,
                              count);
  } else if (pref_name == browsing_data::prefs::kDeletePasswords) {
    StatisticsCallbackSuccess(profiles::kProfileStatisticsPasswords, count);
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

  if (profile_category_stats_.size() ==
      profiles::kProfileStatisticsCategories.size()) {
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
