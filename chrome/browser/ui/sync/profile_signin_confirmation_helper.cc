// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/sync_helper.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

namespace {

const int kHistoryEntriesBeforeNewProfilePrompt = 10;

// Determines whether a profile has any typed URLs in its history.
class HasTypedURLsTask : public history::HistoryDBTask {
 public:
  HasTypedURLsTask(const base::Callback<void(bool)>& cb)
      : has_typed_urls_(false), cb_(cb) {
  }

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    history::URLRows rows;
    backend->GetAllTypedURLs(&rows);
    if (!rows.empty()) {
      DVLOG(1) << "ProfileSigninConfirmationHelper: profile contains "
               << rows.size() << " typed URLs";
      has_typed_urls_ = true;
    }
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    cb_.Run(has_typed_urls_);
  }

 private:
  virtual ~HasTypedURLsTask() {}
  bool has_typed_urls_;
  base::Callback<void(bool)> cb_;
};

bool HasBookmarks(Profile* profile) {
  BookmarkModel* bookmarks = BookmarkModelFactory::GetForProfile(profile);
  bool has_bookmarks = bookmarks && bookmarks->HasBookmarks();
  if (has_bookmarks)
    DVLOG(1) << "ProfileSigninConfirmationHelper: profile contains bookmarks";
  return has_bookmarks;
}

// Helper functions for Chrome profile signin.
class ProfileSigninConfirmationHelper
    : public base::RefCounted<ProfileSigninConfirmationHelper> {
 public:
  ProfileSigninConfirmationHelper(
      Profile* profile,
      const base::Callback<void(bool)>& return_result);
  void CheckHasHistory(int max_entries);
  void CheckHasTypedURLs();
  void set_pending_requests(int requests);

 private:
  friend class base::RefCounted<ProfileSigninConfirmationHelper>;

  ~ProfileSigninConfirmationHelper();

  void OnHistoryQueryResults(size_t max_entries,
                             CancelableRequestProvider::Handle handle,
                             history::QueryResults* results);
  void ReturnResult(bool result);

  // Weak pointer to the profile being signed-in.
  Profile* profile_;

  // Used for async tasks.
  CancelableRequestConsumer request_consumer_;

  // Keep track of how many async requests are pending.
  int pending_requests_;

  // Indicates whether the result has already been returned to caller.
  bool result_returned_;

  // Callback to pass the result back to the caller.
  const base::Callback<void(bool)> return_result_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSigninConfirmationHelper);
};

ProfileSigninConfirmationHelper::ProfileSigninConfirmationHelper(
    Profile* profile,
    const base::Callback<void(bool)>& return_result)
    : profile_(profile),
      pending_requests_(0),
      result_returned_(false),
      return_result_(return_result) {
}

ProfileSigninConfirmationHelper::~ProfileSigninConfirmationHelper() {
}

void ProfileSigninConfirmationHelper::OnHistoryQueryResults(
    size_t max_entries,
    CancelableRequestProvider::Handle handle,
    history::QueryResults* results) {
  history::QueryResults owned_results;
  results->Swap(&owned_results);
  bool too_much_history = owned_results.size() >= max_entries;
  if (too_much_history) {
    DVLOG(1) << "ProfileSigninConfirmationHelper: profile contains "
             << owned_results.size() << " history entries";
  }
  ReturnResult(too_much_history);
}

void ProfileSigninConfirmationHelper::CheckHasHistory(int max_entries) {
  HistoryService* service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile_);
  if (!service) {
    ReturnResult(false);
    return;
  }
  history::QueryOptions opts;
  opts.max_count = max_entries;
  service->QueryHistory(
      base::string16(), opts, &request_consumer_,
      base::Bind(&ProfileSigninConfirmationHelper::OnHistoryQueryResults,
                 this,
                 max_entries));
}

void ProfileSigninConfirmationHelper::CheckHasTypedURLs() {
  HistoryService* service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile_);
  if (!service) {
    ReturnResult(false);
    return;
  }
  service->ScheduleDBTask(
      new HasTypedURLsTask(
          base::Bind(
              &ProfileSigninConfirmationHelper::ReturnResult,
              this)),
      &request_consumer_);
}

void ProfileSigninConfirmationHelper::set_pending_requests(int requests) {
  pending_requests_ = requests;
}

void ProfileSigninConfirmationHelper::ReturnResult(bool result) {
  // Pass |true| into the callback as soon as one of the tasks passes a
  // result of |true|, otherwise pass the last returned result.
  if (!result_returned_ && (--pending_requests_ == 0 || result)) {
    result_returned_ = true;
    request_consumer_.CancelAllRequests();
    return_result_.Run(result);
  }
}

}  // namespace

namespace ui {

SkColor GetSigninConfirmationPromptBarColor(SkAlpha alpha) {
  static const SkColor kBackgroundColor =
      ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground);
  return color_utils::BlendTowardOppositeLuminance(kBackgroundColor, alpha);
}

bool HasBeenShutdown(Profile* profile) {
  bool has_been_shutdown = !profile->IsNewProfile();
  if (has_been_shutdown)
    DVLOG(1) << "ProfileSigninConfirmationHelper: profile is not new";
  return has_been_shutdown;
}

bool HasSyncedExtensions(Profile* profile) {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile);
  if (system && system->extension_service()) {
    const extensions::ExtensionSet* extensions =
        system->extension_service()->extensions();
    for (extensions::ExtensionSet::const_iterator iter = extensions->begin();
         iter != extensions->end(); ++iter) {
      // The webstore is synced so that it stays put on the new tab
      // page, but since it's installed by default we don't want to
      // consider it when determining if the profile is dirty.
      if (extensions::sync_helper::IsSyncable(iter->get()) &&
          (*iter)->id() != extension_misc::kWebStoreAppId &&
          (*iter)->id() != extension_misc::kChromeAppId) {
        DVLOG(1) << "ProfileSigninConfirmationHelper: "
                 << "profile contains a synced extension: " << (*iter)->id();
        return true;
      }
    }
  }
  return false;
}

void CheckShouldPromptForNewProfile(
    Profile* profile,
    const base::Callback<void(bool)>& return_result) {
  if (HasBeenShutdown(profile) ||
      HasBookmarks(profile) ||
      HasSyncedExtensions(profile)) {
    return_result.Run(true);
    return;
  }
  // Fire asynchronous queries for profile data.
  scoped_refptr<ProfileSigninConfirmationHelper> helper =
      new ProfileSigninConfirmationHelper(profile, return_result);
  const int requests = 2;
  helper->set_pending_requests(requests);
  helper->CheckHasHistory(kHistoryEntriesBeforeNewProfilePrompt);
  helper->CheckHasTypedURLs();
}

}  // namespace ui
