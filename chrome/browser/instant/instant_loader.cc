// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_loader.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents_view.h"

namespace {

const int kStalePageTimeoutMS = 3 * 3600 * 1000;  // 3 hours

// This HTTP header and value are set on loads that originate from Instant.
const char kInstantHeader[] = "X-Purpose: Instant";

}  // namespace

InstantLoader::Delegate::~Delegate() {
}

InstantLoader::InstantLoader(Delegate* delegate)
  : delegate_(delegate),
    contents_(NULL),
    stale_page_timer_(false, false) {
}

InstantLoader::~InstantLoader() {
}

void InstantLoader::Init(const GURL& instant_url,
                         Profile* profile,
                         const content::WebContents* active_tab,
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
  contents_->WasHidden();
  stale_page_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kStalePageTimeoutMS),
      on_stale_callback_);
}

void InstantLoader::SetContents(scoped_ptr<content::WebContents> new_contents) {
  contents_.reset(new_contents.release());
  contents_->SetDelegate(this);

  // Set up various tab helpers. The rest will get attached when (if) the
  // contents is added to the tab strip.

  // Tab helpers to control popups.
  BlockedContentTabHelper::CreateForWebContents(contents());
  BlockedContentTabHelper::FromWebContents(contents())->
      SetAllContentsBlocked(true);
  TabSpecificContentSettings::CreateForWebContents(contents());
  TabSpecificContentSettings::FromWebContents(contents())->
      SetPopupsBlocked(true);

  // A tab helper to catch prerender content swapping shenanigans.
  CoreTabHelper::CreateForWebContents(contents());
  CoreTabHelper::FromWebContents(contents())->set_delegate(this);

  // Tab helpers used when committing an overlay.
  chrome::search::SearchTabHelper::CreateForWebContents(contents());
  HistoryTabHelper::CreateForWebContents(contents());

  // Observers.
  extensions::WebNavigationTabObserver::CreateForWebContents(contents());

  // Favicons, required by the Task Manager.
  FaviconTabHelper::CreateForWebContents(contents());

  // And some flat-out paranoia.
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(contents());

#if defined(OS_MACOSX)
  // If |contents_| doesn't yet have a RWHV, SetTakesFocusOnlyOnMouseDown() will
  // be called later, when NOTIFICATION_RENDER_VIEW_HOST_CHANGED is received.
  if (content::RenderWidgetHostView* rwhv =
          contents_->GetRenderWidgetHostView())
    rwhv->SetTakesFocusOnlyOnMouseDown(true);
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::NavigationController>(
                     &contents_->GetController()));
#endif
}

scoped_ptr<content::WebContents> InstantLoader::ReleaseContents() {
  contents_->SetDelegate(NULL);

  // Undo tab helper work done in SetContents().

  BlockedContentTabHelper::FromWebContents(contents())->
      SetAllContentsBlocked(false);
  TabSpecificContentSettings::FromWebContents(contents())->
      SetPopupsBlocked(false);

  CoreTabHelper::FromWebContents(contents())->set_delegate(NULL);

#if defined(OS_MACOSX)
  if (content::RenderWidgetHostView* rwhv =
          contents_->GetRenderWidgetHostView())
    rwhv->SetTakesFocusOnlyOnMouseDown(false);
  registrar_.Remove(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                    content::Source<content::NavigationController>(
                        &contents_->GetController()));
#endif

  return contents_.Pass();
}

void InstantLoader::Observe(int type,
                            const content::NotificationSource& /* source */,
                            const content::NotificationDetails& /* details */) {
#if defined(OS_MACOSX)
  if (type == content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED) {
    if (content::RenderWidgetHostView* rwhv =
            contents_->GetRenderWidgetHostView())
      rwhv->SetTakesFocusOnlyOnMouseDown(true);
    return;
  }
  NOTREACHED();
#endif
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

void InstantLoader::LostCapture() {
  delegate_->OnMouseUp();
}

void InstantLoader::WebContentsFocused(content::WebContents* /* contents */) {
  delegate_->OnFocus();
}

bool InstantLoader::CanDownload(content::RenderViewHost* /* render_view_host */,
                                int /* request_id */,
                                const std::string& /* request_method */) {
  // Downloads are disabled.
  return false;
}

void InstantLoader::HandleMouseDown() {
  delegate_->OnMouseDown();
}

void InstantLoader::HandleMouseUp() {
  delegate_->OnMouseUp();
}

void InstantLoader::HandlePointerActivate() {
  delegate_->OnMouseDown();
}

void InstantLoader::HandleGestureEnd() {
  delegate_->OnMouseUp();
}

void InstantLoader::DragEnded() {
  // If the user drags, we won't get a mouse up (at least on Linux). Commit
  // the Instant result when the drag ends, so that during the drag the page
  // won't move around.
  delegate_->OnMouseUp();
}

bool InstantLoader::OnGoToEntryOffset(int /* offset */) {
  return false;
}

content::WebContents* InstantLoader::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return delegate_->OpenURLFromTab(source, params);
}
