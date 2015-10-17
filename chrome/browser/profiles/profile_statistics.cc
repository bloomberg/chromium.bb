// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

struct ProfileStatValue {
  int count;
  bool success; // false means the statistics failed to load
};

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

class ProfileStatisticsAggregator
    : public base::RefCountedThreadSafe<ProfileStatisticsAggregator> {
  // This class collects statistical information about the profile and returns
  // the information via a callback function. Currently bookmarks, history,
  // logins and preferences are counted.
  //
  // The class is RefCounted because this is needed for CancelableTaskTracker
  // to function properly. Once all tasks are run (or cancelled) the instance is
  // automatically destructed.
  //
  // The class is used internally by GetProfileStatistics function.

 public:
  explicit ProfileStatisticsAggregator(Profile* profile,
      const profiles::ProfileStatisticsCallback& callback,
      base::CancelableTaskTracker* tracker);

 private:
  friend class base::RefCountedThreadSafe<ProfileStatisticsAggregator>;
  ~ProfileStatisticsAggregator() {}

  void Init();

  // Callback functions
  // Normal callback. Appends result to |profile_category_stats_|, and then call
  // the external callback. All other callbacks call this function.
  void StatisticsCallback(const char* category, ProfileStatValue result);
  // Callback for reporting success.
  void StatisticsCallbackSuccess(const char* category, int count);
  // Callback for reporting failure.
  void StatisticsCallbackFailure(const char* category);
  // Callback for history.
  void StatisticsCallbackHistory(history::HistoryCountResult result);

  // Bookmark counting.
  ProfileStatValue CountBookmarks() const;

  // Preference counting.
  ProfileStatValue CountPrefs() const;

  Profile* profile_;
  profiles::ProfileCategoryStats profile_category_stats_;

  // Callback function to be called when results arrive. Will be called
  // multiple times (once for each statistics).
  const profiles::ProfileStatisticsCallback callback_;

  base::CancelableTaskTracker* tracker_;

  // Password counting.
  class PasswordStoreConsumerHelper
      : public password_manager::PasswordStoreConsumer {
   public:
    explicit PasswordStoreConsumerHelper(ProfileStatisticsAggregator* parent)
        : parent_(parent) {}

    void OnGetPasswordStoreResults(
        ScopedVector<autofill::PasswordForm> results) override {
      parent_->StatisticsCallbackSuccess(profiles::kProfileStatisticsPasswords,
                                         results.size());
    }

   private:
    ProfileStatisticsAggregator* parent_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(PasswordStoreConsumerHelper);
  };
  PasswordStoreConsumerHelper password_store_consumer_helper_;

  DISALLOW_COPY_AND_ASSIGN(ProfileStatisticsAggregator);
};

ProfileStatisticsAggregator::ProfileStatisticsAggregator(
    Profile* profile,
    const profiles::ProfileStatisticsCallback& callback,
    base::CancelableTaskTracker* tracker)
    : profile_(profile),
      callback_(callback),
      tracker_(tracker),
      password_store_consumer_helper_(this) {
  Init();
}

void ProfileStatisticsAggregator::Init() {
  // Initiate bookmark counting (async). Post to UI thread.
  tracker_->PostTaskAndReplyWithResult(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI).get(),
          FROM_HERE,
          base::Bind(&ProfileStatisticsAggregator::CountBookmarks, this),
          base::Bind(&ProfileStatisticsAggregator::StatisticsCallback,
                     this, profiles::kProfileStatisticsBookmarks));

  // Initiate history counting (async).
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile_);

  if (history_service) {
    history_service->GetHistoryCount(
        base::Time(),
        base::Time::Max(),
        base::Bind(&ProfileStatisticsAggregator::StatisticsCallbackHistory,
                   this),
        tracker_);
  } else {
    StatisticsCallbackFailure(profiles::kProfileStatisticsBrowsingHistory);
  }

  // Initiate stored password counting (async).
  // TODO(anthonyvd): make password task cancellable.
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (password_store) {
    password_store->GetAutofillableLogins(&password_store_consumer_helper_);
  } else {
    StatisticsCallbackFailure(profiles::kProfileStatisticsPasswords);
  }

  // Initiate preference counting (async). Post to UI thread.
  tracker_->PostTaskAndReplyWithResult(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI).get(),
          FROM_HERE,
          base::Bind(&ProfileStatisticsAggregator::CountPrefs, this),
          base::Bind(&ProfileStatisticsAggregator::StatisticsCallback,
                     this, profiles::kProfileStatisticsSettings));
}

void ProfileStatisticsAggregator::StatisticsCallback(
    const char* category, ProfileStatValue result) {
  profiles::ProfileCategoryStat datum;
  datum.category = category;
  datum.count = result.count;
  datum.success = result.success;
  profile_category_stats_.push_back(datum);
  callback_.Run(profile_category_stats_);
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

ProfileStatValue ProfileStatisticsAggregator::CountBookmarks() const {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfileIfExists(profile_);

  ProfileStatValue result;
  if (bookmark_model) {
    result.count = CountBookmarksFromNode(bookmark_model->bookmark_bar_node()) +
                   CountBookmarksFromNode(bookmark_model->other_node()) +
                   CountBookmarksFromNode(bookmark_model->mobile_node());
    result.success = true;
  } else {
    result.count = 0;
    result.success = false;
  }
  return result;
}

ProfileStatValue ProfileStatisticsAggregator::CountPrefs() const {
  const PrefService* pref_service = profile_->GetPrefs();

  ProfileStatValue result;
  if (pref_service) {
    scoped_ptr<base::DictionaryValue> prefs =
        pref_service->GetPreferenceValuesWithoutPathExpansion();

    int count = 0;
    for (base::DictionaryValue::Iterator it(*(prefs.get()));
         !it.IsAtEnd(); it.Advance()) {
      const PrefService::Preference* pref = pref_service->
                                                FindPreference(it.key());
      // Skip all dictionaries (which must be empty by the function call above).
      if (it.value().GetType() != base::Value::TYPE_DICTIONARY &&
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

}  // namespace

namespace profiles {

// Constants for the categories in ProfileCategoryStats
const char kProfileStatisticsBrowsingHistory[] = "BrowsingHistory";
const char kProfileStatisticsPasswords[] = "Passwords";
const char kProfileStatisticsBookmarks[] = "Bookmarks";
const char kProfileStatisticsSettings[] = "Settings";

void GetProfileStatistics(Profile* profile,
    const ProfileStatisticsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  scoped_refptr<ProfileStatisticsAggregator> aggregator =
      new ProfileStatisticsAggregator(profile, callback, tracker);
}

}  // namespace profiles
