// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

namespace {

bool CanShowBookmarkBar(content::WebUI* ui) {
  if (!ui)
    return false;
  NewTabUI* new_tab = NewTabUI::FromWebUIController(ui->GetController());
  return new_tab && new_tab->CanShowBookmarkBar();
}

}  // namespace

BookmarkTabHelper::BookmarkTabHelper(TabContents* tab_contents)
    : content::WebContentsObserver(tab_contents->web_contents()),
      is_starred_(false),
      tab_contents_(tab_contents),
      delegate_(NULL),
      bookmark_drag_(NULL) {
  // Register for notifications about URL starredness changing on any profile.
  registrar_.Add(this, chrome::NOTIFICATION_URLS_STARRED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

BookmarkTabHelper::~BookmarkTabHelper() {
  // We don't want any notifications while we're running our destructor.
  registrar_.RemoveAll();
}

bool BookmarkTabHelper::ShouldShowBookmarkBar() {
  if (web_contents()->ShowingInterstitialPage())
    return false;

  // See WebContents::GetWebUIForCurrentState() comment for more info. This case
  // is very similar, but for non-first loads, we want to use the committed
  // entry. This is so the bookmarks bar disappears at the same time the page
  // does.
  if (web_contents()->GetController().GetLastCommittedEntry()) {
    // Not the first load, always use the committed Web UI.
    return CanShowBookmarkBar(web_contents()->GetCommittedWebUI());
  }

  // When it's the first load, we know either the pending one or the committed
  // one will have the Web UI in it (see GetWebUIForCurrentState), and only one
  // of them will be valid, so we can just check both.
  return CanShowBookmarkBar(web_contents()->GetWebUI());
}

void BookmarkTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& /*details*/,
    const content::FrameNavigateParams& /*params*/) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED:
      // BookmarkModel finished loading, fall through to update starred state.
    case chrome::NOTIFICATION_URLS_STARRED: {
      // Somewhere, a URL has been starred.
      // Ignore notifications for profiles other than our current one.
      Profile* source_profile = content::Source<Profile>(source).ptr();
      if (!source_profile ||
          !source_profile->IsSameProfile(tab_contents_->profile()))
        return;

      UpdateStarredStateForCurrentURL();
      break;
    }

    default:
      NOTREACHED();
  }
}

void BookmarkTabHelper::SetBookmarkDragDelegate(
    BookmarkTabHelper::BookmarkDrag* bookmark_drag) {
  bookmark_drag_ = bookmark_drag;
}

BookmarkTabHelper::BookmarkDrag*
    BookmarkTabHelper::GetBookmarkDragDelegate() {
  return bookmark_drag_;
}

void BookmarkTabHelper::UpdateStarredStateForCurrentURL() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  BookmarkModel* model = profile->GetBookmarkModel();
  const bool old_state = is_starred_;
  is_starred_ = (model && model->IsBookmarked(web_contents()->GetURL()));

  if (is_starred_ != old_state && delegate())
    delegate()->URLStarredChanged(tab_contents_, is_starred_);
}
