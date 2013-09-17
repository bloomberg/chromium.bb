// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_loader.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#endif

namespace {

// This HTTP header and value are set on loads that originate from Instant.
const char kInstantHeader[] = "X-Purpose: Instant";

}  // namespace

InstantLoader::Delegate::~Delegate() {
}

InstantLoader::InstantLoader(Delegate* delegate)
    : delegate_(delegate), stale_page_timer_(false, false) {}

InstantLoader::~InstantLoader() {
}

void InstantLoader::Init(const GURL& instant_url,
                         Profile* profile,
                         const base::Closure& on_stale_callback) {
  content::WebContents::CreateParams create_params(profile);
  create_params.site_instance = content::SiteInstance::CreateForURL(
      profile, instant_url);
  SetContents(scoped_ptr<content::WebContents>(
      content::WebContents::Create(create_params)));
  instant_url_ = instant_url;
  on_stale_callback_ = on_stale_callback;
}

void InstantLoader::Load() {
  DVLOG(1) << "LoadURL: " << instant_url_;
  contents_->GetController().LoadURL(
      instant_url_, content::Referrer(),
      content::PAGE_TRANSITION_GENERATED, kInstantHeader);

  // Explicitly set the new tab title and virtual URL.
  //
  // This ensures that the title is set even before we get a title from the
  // page, preventing a potential flicker of the URL, and also ensures that
  // (unless overridden by the page) the new tab title matches the browser UI
  // locale.
  content::NavigationEntry* entry = contents_->GetController().GetActiveEntry();
  if (entry)
    entry->SetTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));

  contents_->WasHidden();

  int staleness_timeout_ms = chrome::GetInstantLoaderStalenessTimeoutSec() *
      1000;
  if (staleness_timeout_ms > 0) {
    stale_page_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(staleness_timeout_ms),
        on_stale_callback_);
  }
}

void InstantLoader::SetContents(scoped_ptr<content::WebContents> new_contents) {
  contents_.reset(new_contents.release());
  contents_->SetDelegate(this);

  // Set up various tab helpers. The rest will get attached when (if) the
  // contents is added to the tab strip.

  // Bookmarks (Users can bookmark the Instant NTP. This ensures the bookmarked
  // state is correctly set when the contents are swapped into a tab.)
  BookmarkTabHelper::CreateForWebContents(contents());

  // A tab helper to catch prerender content swapping shenanigans.
  CoreTabHelper::CreateForWebContents(contents());
  CoreTabHelper::FromWebContents(contents())->set_delegate(this);

  SearchTabHelper::CreateForWebContents(contents());

#if !defined(OS_ANDROID)
  // Observers.
  extensions::WebNavigationTabObserver::CreateForWebContents(contents());
#endif  // OS_ANDROID

  // Favicons, required by the Task Manager.
  FaviconTabHelper::CreateForWebContents(contents());

  // And some flat-out paranoia.
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(contents());

  // When the WebContents finishes loading it should be checked to ensure that
  // it is in the instant process.
  registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                 content::Source<content::WebContents>(contents_.get()));
}

scoped_ptr<content::WebContents> InstantLoader::ReleaseContents() {
  stale_page_timer_.Stop();
  contents_->SetDelegate(NULL);

  // Undo tab helper work done in SetContents().
  CoreTabHelper::FromWebContents(contents())->set_delegate(NULL);

  registrar_.Remove(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                    content::Source<content::WebContents>(contents_.get()));
  return contents_.Pass();
}

void InstantLoader::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME);
  const content::WebContents* web_contents =
      content::Source<content::WebContents>(source).ptr();
  DCHECK_EQ(contents_.get(), web_contents);
  delegate_->LoadCompletedMainFrame();
}

void InstantLoader::SwapTabContents(content::WebContents* old_contents,
                                    content::WebContents* new_contents) {
  DCHECK_EQ(old_contents, contents());
  // We release here without deleting since the caller has the responsibility
  // for deleting the old WebContents.
  ignore_result(ReleaseContents().release());
  SetContents(scoped_ptr<content::WebContents>(new_contents));
  delegate_->OnSwappedContents();
}

bool InstantLoader::ShouldSuppressDialogs() {
  // Messages shown during Instant cancel Instant, so we suppress them.
  return true;
}

bool InstantLoader::ShouldFocusPageAfterCrash() {
  return false;
}

void InstantLoader::CanDownload(content::RenderViewHost* /* render_view_host */,
                                int /* request_id */,
                                const std::string& /* request_method */,
                                const base::Callback<void(bool)>& callback) {
  // Downloads are disabled.
  callback.Run(false);
}

bool InstantLoader::OnGoToEntryOffset(int /* offset */) {
  return false;
}

content::WebContents* InstantLoader::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return delegate_->OpenURLFromTab(source, params);
}
