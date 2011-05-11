// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/common/notification_service.h"

BookmarkTabHelper::BookmarkTabHelper(TabContentsWrapper* tab_contents)
    : TabContentsObserver(tab_contents->tab_contents()),
      is_starred_(false),
      tab_contents_wrapper_(tab_contents),
      delegate_(NULL) {
  // Register for notifications about URL starredness changing on any profile.
  registrar_.Add(this, NotificationType::URLS_STARRED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                 NotificationService::AllSources());
}

BookmarkTabHelper::~BookmarkTabHelper() {
  // We don't want any notifications while we're running our destructor.
  registrar_.RemoveAll();
}

void BookmarkTabHelper::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& /*details*/,
    const ViewHostMsg_FrameNavigate_Params& /*params*/) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::BOOKMARK_MODEL_LOADED:
      // BookmarkModel finished loading, fall through to update starred state.
    case NotificationType::URLS_STARRED: {
      // Somewhere, a URL has been starred.
      // Ignore notifications for profiles other than our current one.
      Profile* source_profile = Source<Profile>(source).ptr();
      if (!source_profile ||
          !source_profile->IsSameProfile(tab_contents_wrapper_->profile()))
        return;

      UpdateStarredStateForCurrentURL();
      break;
    }

    default:
      NOTREACHED();
  }
}

void BookmarkTabHelper::UpdateStarredStateForCurrentURL() {
  BookmarkModel* model = tab_contents()->profile()->GetBookmarkModel();
  const bool old_state = is_starred_;
  is_starred_ = (model && model->IsBookmarked(tab_contents()->GetURL()));

  if (is_starred_ != old_state && delegate())
    delegate()->URLStarredChanged(tab_contents_wrapper_, is_starred_);
}
