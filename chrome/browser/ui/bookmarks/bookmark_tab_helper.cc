// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_delegate.h"
#include "chrome/browser/ui/sad_tab.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace {

bool IsNTPWebUI(content::WebContents* web_contents) {
  content::WebUI* web_ui = NULL;
  // Use the committed entry so the bookmarks bar disappears at the same time
  // the page does.
  if (web_contents->GetController().GetLastCommittedEntry())
    web_ui = web_contents->GetCommittedWebUI();
  else
    web_ui = web_contents->GetWebUI();
  return web_ui && NewTabUI::FromWebUIController(web_ui->GetController());
}

bool IsInstantNTP(content::WebContents* web_contents) {
  // Use the committed entry so the bookmarks bar disappears at the same time
  // the page does.
  const content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry)
    entry = web_contents->GetController().GetVisibleEntry();
  return chrome::search::NavEntryIsInstantNTP(web_contents, entry);
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(BookmarkTabHelper);

BookmarkTabHelper::~BookmarkTabHelper() {
  if (bookmark_model_)
    bookmark_model_->RemoveObserver(this);
}

bool BookmarkTabHelper::ShouldShowBookmarkBar() const {
  if (web_contents()->ShowingInterstitialPage())
    return false;

  if (chrome::SadTab::ShouldShow(web_contents()->GetCrashedStatus()))
    return false;

  if (!browser_defaults::bookmarks_enabled)
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  if (prefs->IsManagedPreference(prefs::kShowBookmarkBar) &&
      !prefs->GetBoolean(prefs::kShowBookmarkBar))
    return false;

  return IsNTPWebUI(web_contents()) || IsInstantNTP(web_contents());
}

BookmarkTabHelper::BookmarkTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      is_starred_(false),
      bookmark_model_(NULL),
      delegate_(NULL),
      bookmark_drag_(NULL) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bookmark_model_= BookmarkModelFactory::GetForProfile(profile);
  if (bookmark_model_)
    bookmark_model_->AddObserver(this);
}

void BookmarkTabHelper::UpdateStarredStateForCurrentURL() {
  const bool old_state = is_starred_;
  is_starred_ = (bookmark_model_ &&
                 bookmark_model_->IsBookmarked(web_contents()->GetURL()));

  if (is_starred_ != old_state && delegate_)
    delegate_->URLStarredChanged(web_contents(), is_starred_);
}

void BookmarkTabHelper::BookmarkModelChanged() {
}

void BookmarkTabHelper::Loaded(BookmarkModel* model, bool ids_reassigned) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkNodeAdded(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int index) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkNodeRemoved(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int old_index,
                                            const BookmarkNode* node) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkNodeChanged(BookmarkModel* model,
                                            const BookmarkNode* node) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& /*details*/,
    const content::FrameNavigateParams& /*params*/) {
  UpdateStarredStateForCurrentURL();
}
