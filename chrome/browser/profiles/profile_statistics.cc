// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics.h"

#include <set>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

struct ProfileStatValue {
  int count;
  bool success;  // false means the statistics failed to load
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
  // This class is used internally by GetProfileStatistics and
  // StoreProfileStatisticsToCache.
  //
  // The class collects statistical information about the profile and returns
  // the information via a callback function. Currently bookmarks, history,
  // logins and preferences are counted.
  //
  // The class is RefCounted because this is needed for CancelableTaskTracker
  // to function properly. Once all tasks are run (or cancelled) the instance is
  // automatically destructed.

 public:
  ProfileStatisticsAggregator(Profile* profile,
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
  void WaitOrCountBookmarks();
  void CountBookmarks(bookmarks::BookmarkModel* bookmark_model);

  class BookmarkModelHelper
      : public bookmarks::BookmarkModelObserver {
   public:
    explicit BookmarkModelHelper(ProfileStatisticsAggregator* parent)
        : parent_(parent) {}

    void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                             bool ids_reassigned)
        override {
      // Remove observer before release, otherwise it may become a dangling
      // reference.
      model->RemoveObserver(this);
      parent_->CountBookmarks(model);
      parent_->Release();
    }

    void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* old_parent,
                           int old_index,
                           const bookmarks::BookmarkNode* new_parent,
                           int new_index) override {}

    void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           int index) override {}

    void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
        const bookmarks::BookmarkNode* parent,
        int old_index, const bookmarks::BookmarkNode* node,
        const std::set<GURL>& no_longer_bookmarked) override {}

    void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
        const bookmarks::BookmarkNode* node) override {}

    void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
        const bookmarks::BookmarkNode* node) override {}

    void BookmarkNodeChildrenReordered(bookmarks::BookmarkModel* model,
        const bookmarks::BookmarkNode* node) override {}

    void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
        const std::set<GURL>& removed_urls) override {}

   private:
    ProfileStatisticsAggregator* parent_ = nullptr;
  };

  // Password counting
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

  // Preference counting.
  ProfileStatValue CountPrefs() const;

  Profile* profile_;
  profiles::ProfileCategoryStats profile_category_stats_;

  // Callback function to be called when results arrive. Will be called
  // multiple times (once for each statistics).
  const profiles::ProfileStatisticsCallback callback_;

  base::CancelableTaskTracker* tracker_;
  scoped_ptr<base::CancelableTaskTracker> default_tracker_;

  // Bookmark counting
  scoped_ptr<BookmarkModelHelper> bookmark_model_helper_;

  // Password counting.
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
  if (!tracker_) {
    default_tracker_.reset(new base::CancelableTaskTracker);
    tracker_ = default_tracker_.get();
  }
  Init();
}

void ProfileStatisticsAggregator::Init() {
  DCHECK(profile_);

  // Initiate bookmark counting (async). Post to UI thread.
  tracker_->PostTask(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI).get(),
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
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI).get(),
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
  if (!callback_.is_null())
    callback_.Run(profile_category_stats_);

  if (result.success) {
    profiles::SetProfileStatisticsInCache(profile_->GetPath(), datum.category,
                                          result.count);
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
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfileIfExists(profile_);

  if (bookmark_model) {
    if (bookmark_model->loaded()) {
      CountBookmarks(bookmark_model);
    } else {
      AddRef();
      bookmark_model_helper_.reset(new BookmarkModelHelper(this));
      bookmark_model->AddObserver(bookmark_model_helper_.get());
    }
  } else {
    StatisticsCallbackFailure(profiles::kProfileStatisticsBookmarks);
  }
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

void GatherProfileStatistics(Profile* profile,
                             const ProfileStatisticsCallback& callback,
                             base::CancelableTaskTracker* tracker) {
  DCHECK(profile);
  if (profile->IsOffTheRecord() || profile->IsSystemProfile()) {
    NOTREACHED();
    return;
  }

  scoped_refptr<ProfileStatisticsAggregator> aggregator =
      new ProfileStatisticsAggregator(profile, callback, tracker);
}

ProfileCategoryStats GetProfileStatisticsFromCache(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry = nullptr;
  bool has_entry = g_browser_process->profile_manager()->
      GetProfileAttributesStorage().
      GetProfileAttributesWithPath(profile_path, &entry);

  ProfileCategoryStats stats;
  ProfileCategoryStat stat;

  stat.category = kProfileStatisticsBrowsingHistory;
  stat.success = has_entry ? entry->HasStatsBrowsingHistory() : false;
  stat.count = stat.success ? entry->GetStatsBrowsingHistory() : 0;
  stats.push_back(stat);

  stat.category = kProfileStatisticsPasswords;
  stat.success = has_entry ? entry->HasStatsPasswords() : false;
  stat.count = stat.success ? entry->GetStatsPasswords() : 0;
  stats.push_back(stat);

  stat.category = kProfileStatisticsBookmarks;
  stat.success = has_entry ? entry->HasStatsBookmarks() : false;
  stat.count = stat.success ? entry->GetStatsBookmarks() : 0;
  stats.push_back(stat);

  stat.category = kProfileStatisticsSettings;
  stat.success = has_entry ? entry->HasStatsSettings() : false;
  stat.count = stat.success ? entry->GetStatsSettings() : 0;
  stats.push_back(stat);

  return stats;
}

void SetProfileStatisticsInCache(const base::FilePath& profile_path,
                                 const std::string& category, int count) {
  // If local_state() is null, profile_manager() will seg-fault.
  if (!g_browser_process || !g_browser_process->local_state())
    return;

  // profile_manager() may return a null pointer.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return;

  ProfileAttributesEntry* entry = nullptr;
  if (!profile_manager->GetProfileAttributesStorage().
      GetProfileAttributesWithPath(profile_path, &entry))
    return;

  if (category == kProfileStatisticsBrowsingHistory) {
    entry->SetStatsBrowsingHistory(count);
  } else if (category == kProfileStatisticsPasswords) {
    entry->SetStatsPasswords(count);
  } else if (category == kProfileStatisticsBookmarks) {
    entry->SetStatsBookmarks(count);
  } else if (category == kProfileStatisticsSettings) {
    entry->SetStatsSettings(count);
  } else {
    NOTREACHED();
  }
}

}  // namespace profiles
