// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics.h"

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_statistics_aggregator.h"

ProfileStatistics::ProfileStatistics(Profile* profile)
    : profile_(profile), aggregator_(nullptr), weak_ptr_factory_(this) {
}

ProfileStatistics::~ProfileStatistics() {
}

void ProfileStatistics::GatherStatistics(
    const profiles::ProfileStatisticsCallback& callback) {
  // IsValidProfile() can be false in unit tests.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_))
    return;
  DCHECK(!profile_->IsOffTheRecord() && !profile_->IsSystemProfile());

  if (HasAggregator()) {
    GetAggregator()->AddCallbackAndStartAggregator(callback);
  } else {
    // The statistics task may outlive ProfileStatistics in unit tests, so a
    // weak pointer is used for the callback.
    scoped_refptr<ProfileStatisticsAggregator> aggregator =
        new ProfileStatisticsAggregator(
                profile_,
                callback,
                base::Bind(&ProfileStatistics::DeregisterAggregator,
                           weak_ptr_factory_.GetWeakPtr()));
    RegisterAggregator(aggregator.get());
  }
}

bool ProfileStatistics::HasAggregator() const {
  return aggregator_ != nullptr;
}

ProfileStatisticsAggregator* ProfileStatistics::GetAggregator() const {
  return aggregator_;
}

void ProfileStatistics::RegisterAggregator(
    ProfileStatisticsAggregator* aggregator) {
  aggregator_ = aggregator;
}

void ProfileStatistics::DeregisterAggregator() {
  aggregator_ = nullptr;
}

// static
profiles::ProfileCategoryStats
    ProfileStatistics::GetProfileStatisticsFromAttributesStorage(
        const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry = nullptr;
  bool has_entry = g_browser_process->profile_manager()->
      GetProfileAttributesStorage().
      GetProfileAttributesWithPath(profile_path, &entry);

  profiles::ProfileCategoryStats stats;
  profiles::ProfileCategoryStat stat;

  stat.category = profiles::kProfileStatisticsBrowsingHistory;
  stat.success = has_entry ? entry->HasStatsBrowsingHistory() : false;
  stat.count = stat.success ? entry->GetStatsBrowsingHistory() : 0;
  stats.push_back(stat);

  stat.category = profiles::kProfileStatisticsPasswords;
  stat.success = has_entry ? entry->HasStatsPasswords() : false;
  stat.count = stat.success ? entry->GetStatsPasswords() : 0;
  stats.push_back(stat);

  stat.category = profiles::kProfileStatisticsBookmarks;
  stat.success = has_entry ? entry->HasStatsBookmarks() : false;
  stat.count = stat.success ? entry->GetStatsBookmarks() : 0;
  stats.push_back(stat);

  stat.category = profiles::kProfileStatisticsSettings;
  stat.success = has_entry ? entry->HasStatsSettings() : false;
  stat.count = stat.success ? entry->GetStatsSettings() : 0;
  stats.push_back(stat);

  return stats;
}

// static
void ProfileStatistics::SetProfileStatisticsToAttributesStorage(
    const base::FilePath& profile_path,
    const std::string& category,
    int count) {
  // |profile_manager()| may return a null pointer.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return;

  ProfileAttributesEntry* entry = nullptr;
  if (!profile_manager->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_path, &entry)) {
    // It is possible to have the profile attributes entry absent, e.g. the
    // profile is scheduled for deletion during the async statistics task.
    return;
  }

  if (category == profiles::kProfileStatisticsBrowsingHistory) {
    entry->SetStatsBrowsingHistory(count);
  } else if (category == profiles::kProfileStatisticsPasswords) {
    entry->SetStatsPasswords(count);
  } else if (category == profiles::kProfileStatisticsBookmarks) {
    entry->SetStatsBookmarks(count);
  } else if (category == profiles::kProfileStatisticsSettings) {
    entry->SetStatsSettings(count);
  } else {
    NOTREACHED();
  }
}
