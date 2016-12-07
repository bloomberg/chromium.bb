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
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

int CountBookmarksFromNode(const bookmarks::BookmarkNode* node) {
  int count = 0;
  if (node->is_url()) {
    ++count;
  } else {
    for (int i = 0; i < node->child_count(); ++i)
      count += CountBookmarksFromNode(node->GetChild(i));
  }
  return count;
}

}  // namespace

void ProfileStatisticsAggregator::BookmarkModelHelper::BookmarkModelLoaded(
    bookmarks::BookmarkModel* model, bool ids_reassigned) {
  // Remove observer before release, otherwise it may become a dangling
  // reference.
  model->RemoveObserver(this);
  parent_->CountBookmarks(model);
  parent_->Release();
}

void ProfileStatisticsAggregator::PasswordStoreConsumerHelper::
    OnGetPasswordStoreResults(
        std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  parent_->StatisticsCallbackSuccess(
      profiles::kProfileStatisticsPasswords, results.size());
}

ProfileStatisticsAggregator::ProfileStatisticsAggregator(
    Profile* profile, const profiles::ProfileStatisticsCallback& stats_callback,
    const base::Closure& destruct_callback)
    : profile_(profile),
      profile_path_(profile_->GetPath()),
      destruct_callback_(destruct_callback),
      password_store_consumer_helper_(this) {
  AddCallbackAndStartAggregator(stats_callback);
}

ProfileStatisticsAggregator::~ProfileStatisticsAggregator() {
  if (!destruct_callback_.is_null())
    destruct_callback_.Run();
}

size_t ProfileStatisticsAggregator::GetCallbackCount() {
  return stats_callbacks_.size();
}

void ProfileStatisticsAggregator::AddCallbackAndStartAggregator(
    const profiles::ProfileStatisticsCallback& stats_callback) {
  if (!stats_callback.is_null())
    stats_callbacks_.push_back(stats_callback);
  StartAggregator();
}

void ProfileStatisticsAggregator::StartAggregator() {
  DCHECK(g_browser_process->profile_manager()->IsValidProfile(profile_));
  profile_category_stats_.clear();

  // Try to cancel tasks from task trackers.
  tracker_.TryCancelAll();
  password_store_consumer_helper_.cancelable_task_tracker()->TryCancelAll();

  // Initiate bookmark counting (async).
  tracker_.PostTask(
      content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
          .get(),
      FROM_HERE,
      base::Bind(&ProfileStatisticsAggregator::WaitOrCountBookmarks, this));

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

void ProfileStatisticsAggregator::StatisticsCallback(
    const char* category, ProfileStatValue result) {
  profiles::ProfileCategoryStat datum;
  datum.category = category;
  datum.count = result.count;
  datum.success = result.success;
  profile_category_stats_.push_back(datum);
  for (const auto& stats_callback : stats_callbacks_) {
    DCHECK(!stats_callback.is_null());
    stats_callback.Run(profile_category_stats_);
  }

  if (result.success) {
    ProfileStatistics::SetProfileStatisticsToAttributesStorage(
        profile_path_, datum.category, result.count);
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

void ProfileStatisticsAggregator::CountBookmarks(
    bookmarks::BookmarkModel* bookmark_model) {
  int count = CountBookmarksFromNode(bookmark_model->bookmark_bar_node()) +
              CountBookmarksFromNode(bookmark_model->other_node()) +
              CountBookmarksFromNode(bookmark_model->mobile_node());

  StatisticsCallbackSuccess(profiles::kProfileStatisticsBookmarks, count);
}

void ProfileStatisticsAggregator::WaitOrCountBookmarks() {
  // The following checks should only fail in unit tests unrelated to gathering
  // statistics. Do not bother to return failure in any of these cases.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return;
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_))
    return;

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContextIfExists(profile_);

  if (bookmark_model) {
    if (bookmark_model->loaded()) {
      CountBookmarks(bookmark_model);
    } else if (!bookmark_model_helper_) {
      // If |bookmark_model_helper_| is not null, it means a previous bookmark
      // counting task still waiting for the bookmark model to load. Do nothing
      // and continue to use the old |bookmark_model_helper_| in this case.
      AddRef();
      bookmark_model_helper_.reset(new BookmarkModelHelper(this));
      bookmark_model->AddObserver(bookmark_model_helper_.get());
    }
  } else {
    StatisticsCallbackFailure(profiles::kProfileStatisticsBookmarks);
  }
}

ProfileStatisticsAggregator::ProfileStatValue
    ProfileStatisticsAggregator::CountPrefs() const {
  const PrefService* pref_service = profile_->GetPrefs();

  ProfileStatValue result;
  if (pref_service) {
    std::unique_ptr<base::DictionaryValue> prefs =
        pref_service->GetPreferenceValuesWithoutPathExpansion();

    int count = 0;
    for (base::DictionaryValue::Iterator it(*(prefs.get()));
         !it.IsAtEnd(); it.Advance()) {
      const PrefService::Preference* pref = pref_service->
                                                FindPreference(it.key());
      // Skip all dictionaries (which must be empty by the function call above).
      if (it.value().GetType() != base::Value::Type::DICTIONARY &&
        pref && pref->IsUserControlled() && !pref->IsDefaultValue()) {
        ++count;
      }
    }

    result.count = count;
    result.success = true;
  } else {
    result.count = 0;
    result.success = false;
  }
  return result;
}
