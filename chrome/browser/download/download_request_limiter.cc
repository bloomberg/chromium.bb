// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_limiter.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/download/download_request_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

// TabDownloadState ------------------------------------------------------------

DownloadRequestLimiter::TabDownloadState::TabDownloadState(
    DownloadRequestLimiter* host,
    WebContents* contents,
    WebContents* originating_web_contents)
    : content::WebContentsObserver(contents),
      host_(host),
      status_(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD),
      download_count_(0),
      infobar_(NULL) {
  content::Source<NavigationController> notification_source(
      &contents->GetController());
  content::Source<content::WebContents> web_contents_source(contents);
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                 notification_source);
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 web_contents_source);

  NavigationEntry* active_entry = originating_web_contents ?
      originating_web_contents->GetController().GetActiveEntry() :
      contents->GetController().GetActiveEntry();
  if (active_entry)
    initial_page_host_ = active_entry->GetURL().host();
}

DownloadRequestLimiter::TabDownloadState::~TabDownloadState() {
  // We should only be destroyed after the callbacks have been notified.
  DCHECK(callbacks_.empty());

  // And we should have closed the infobar.
  DCHECK(!infobar_);
}

void DownloadRequestLimiter::TabDownloadState::DidGetUserGesture() {
  if (is_showing_prompt()) {
    // Don't change the state if the user clicks on the page somewhere.
    return;
  }

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  // See PromptUserForDownload(): if there's no InfoBarService, then
  // DOWNLOADS_NOT_ALLOWED is functionally equivalent to PROMPT_BEFORE_DOWNLOAD.
  if ((infobar_service &&
       status_ != DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS &&
       status_ != DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED) ||
      (!infobar_service &&
       status_ != DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS)) {
    // Revert to default status.
    host_->Remove(this);
    // WARNING: We've been deleted.
  }
}

void DownloadRequestLimiter::TabDownloadState::PromptUserForDownload(
    WebContents* web_contents,
    const DownloadRequestLimiter::Callback& callback) {
  callbacks_.push_back(callback);

  if (is_showing_prompt())
    return;  // Already showing prompt.

  if (DownloadRequestLimiter::delegate_) {
    NotifyCallbacks(DownloadRequestLimiter::delegate_->ShouldAllowDownload());
    return;
  }

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  if (!infobar_service) {
    // |web_contents| may not have a InfoBarService if it's actually a
    // WebContents like those used for extension popups/bubbles and hosted apps
    // etc.
    // TODO(benjhayden): If this is an automatic download from an extension,
    // it would be convenient for the extension author if we send a message to
    // the extension's DevTools console (as we do for CSP) about how
    // extensions should use chrome.downloads.download() (requires the
    // "downloads" permission) to automatically download >1 files.
    Cancel();
    return;
  }
  DownloadRequestInfoBarDelegate::Create(infobar_service, this);
}

void DownloadRequestLimiter::TabDownloadState::Cancel() {
  NotifyCallbacks(false);
}

void DownloadRequestLimiter::TabDownloadState::Accept() {
  NotifyCallbacks(true);
}

DownloadRequestLimiter::TabDownloadState::TabDownloadState()
    : host_(NULL),
      status_(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD),
      download_count_(0),
      infobar_(NULL) {
}

void DownloadRequestLimiter::TabDownloadState::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  content::NavigationController* controller = &web_contents()->GetController();
  if (type == content::NOTIFICATION_NAV_ENTRY_PENDING) {
    DCHECK_EQ(controller, content::Source<NavigationController>(source).ptr());

    // NOTE: Resetting state on a pending navigate isn't ideal. In particular it
    // is possible that queued up downloads for the page before the pending
    // navigation will be delivered to us after we process this request. If this
    // happens we may let a download through that we shouldn't have. But this is
    // rather rare, and it is difficult to get 100% right, so we don't deal with
    // it.
    NavigationEntry* entry = controller->GetPendingEntry();
    if (!entry)
      return;

    // Redirects don't count.
    if (content::PageTransitionIsRedirect(entry->GetTransitionType()))
      return;

    if (status_ == DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS ||
        status_ == DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED) {
      // User has either allowed all downloads or canceled all downloads. Only
      // reset the download state if the user is navigating to a different host
      // (or host is empty).
      if (!initial_page_host_.empty() && !entry->GetURL().host().empty() &&
          entry->GetURL().host() == initial_page_host_)
        return;
    }
  } else {
    DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DESTROYED, type);
    DCHECK_EQ(controller,
              &content::Source<content::WebContents>(source)->GetController());
    // Tab closed, no need to handle closing the dialog as it's owned by the
    // WebContents.
  }

  NotifyCallbacks(false);
  host_->Remove(this);
}

void DownloadRequestLimiter::TabDownloadState::NotifyCallbacks(bool allow) {
  set_download_status(allow ?
      DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS :
      DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);
  std::vector<DownloadRequestLimiter::Callback> callbacks;
  bool change_status = false;

  // Selectively send first few notifications only if number of downloads exceed
  // kMaxDownloadsAtOnce. In that case, we also retain the infobar instance and
  // don't close it. If allow is false, we send all the notifications to cancel
  // all remaining downloads and close the infobar.
  if (!allow || (callbacks_.size() < kMaxDownloadsAtOnce)) {
    if (infobar_) {
      // Reset the delegate so we don't get notified again.
      infobar_->set_host(NULL);
      infobar_ = NULL;
    }
    callbacks.swap(callbacks_);
  } else {
    std::vector<DownloadRequestLimiter::Callback>::iterator start, end;
    start = callbacks_.begin();
    end = callbacks_.begin() + kMaxDownloadsAtOnce;
    callbacks.assign(start, end);
    callbacks_.erase(start, end);
    change_status = true;
  }

  for (size_t i = 0; i < callbacks.size(); ++i)
    host_->ScheduleNotification(callbacks[i], allow);

  if (change_status)
    set_download_status(DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD);
}

// DownloadRequestLimiter ------------------------------------------------------

DownloadRequestLimiter::DownloadRequestLimiter() {
}

DownloadRequestLimiter::~DownloadRequestLimiter() {
  // All the tabs should have closed before us, which sends notification and
  // removes from state_map_. As such, there should be no pending callbacks.
  DCHECK(state_map_.empty());
}

DownloadRequestLimiter::DownloadStatus
    DownloadRequestLimiter::GetDownloadStatus(WebContents* web_contents) {
  TabDownloadState* state = GetDownloadState(web_contents, NULL, false);
  return state ? state->download_status() : ALLOW_ONE_DOWNLOAD;
}

void DownloadRequestLimiter::CanDownloadOnIOThread(
    int render_process_host_id,
    int render_view_id,
    int request_id,
    const std::string& request_method,
    const Callback& callback) {
  // This is invoked on the IO thread. Schedule the task to run on the UI
  // thread so that we can query UI state.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadRequestLimiter::CanDownload, this,
                 render_process_host_id, render_view_id, request_id,
                 request_method, callback));
}

// static
void DownloadRequestLimiter::SetTestingDelegate(TestingDelegate* delegate) {
  delegate_ = delegate;
}

DownloadRequestLimiter::TabDownloadState*
DownloadRequestLimiter::GetDownloadState(
    WebContents* web_contents,
    WebContents* originating_web_contents,
    bool create) {
  DCHECK(web_contents);
  StateMap::iterator i = state_map_.find(web_contents);
  if (i != state_map_.end())
    return i->second;

  if (!create)
    return NULL;

  TabDownloadState* state =
      new TabDownloadState(this, web_contents, originating_web_contents);
  state_map_[web_contents] = state;
  return state;
}

void DownloadRequestLimiter::CanDownload(int render_process_host_id,
                                         int render_view_id,
                                         int request_id,
                                         const std::string& request_method,
                                         const Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WebContents* originating_contents =
      tab_util::GetWebContentsByID(render_process_host_id, render_view_id);
  if (!originating_contents) {
    // The WebContents was closed, don't allow the download.
    ScheduleNotification(callback, false);
    return;
  }

  CanDownloadImpl(
      originating_contents,
      request_id,
      request_method,
      callback);
}

void DownloadRequestLimiter::CanDownloadImpl(WebContents* originating_contents,
                                             int request_id,
                                             const std::string& request_method,
                                             const Callback& callback) {
  DCHECK(originating_contents);

  // FYI: Chrome Frame overrides CanDownload in ExternalTabContainer in order
  // to cancel the download operation in chrome and let the host browser
  // take care of it.
  if (originating_contents->GetDelegate() &&
      !originating_contents->GetDelegate()->CanDownload(
          originating_contents->GetRenderViewHost(),
          request_id,
          request_method)) {
    ScheduleNotification(callback, false);
    return;
  }

  // If the tab requesting the download is a constrained popup that is not
  // shown, treat the request as if it came from the parent.
  WebContents* effective_contents = originating_contents;
  BlockedContentTabHelper* blocked_content_tab_helper =
      BlockedContentTabHelper::FromWebContents(originating_contents);
  if (blocked_content_tab_helper &&
      blocked_content_tab_helper->delegate()) {
    effective_contents = blocked_content_tab_helper->delegate()->
        GetConstrainingWebContents(originating_contents);
  }

  TabDownloadState* state = GetDownloadState(
      effective_contents, originating_contents, true);
  switch (state->download_status()) {
    case ALLOW_ALL_DOWNLOADS:
      if (state->download_count() && !(state->download_count() %
            DownloadRequestLimiter::kMaxDownloadsAtOnce))
        state->set_download_status(PROMPT_BEFORE_DOWNLOAD);
      ScheduleNotification(callback, true);
      state->increment_download_count();
      break;

    case ALLOW_ONE_DOWNLOAD:
      state->set_download_status(PROMPT_BEFORE_DOWNLOAD);
      ScheduleNotification(callback, true);
      break;

    case DOWNLOADS_NOT_ALLOWED:
      ScheduleNotification(callback, false);
      break;

    case PROMPT_BEFORE_DOWNLOAD:
      state->PromptUserForDownload(effective_contents, callback);
      state->increment_download_count();
      break;

    default:
      NOTREACHED();
  }
}

void DownloadRequestLimiter::ScheduleNotification(const Callback& callback,
                                                  bool allow) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, allow));
}

void DownloadRequestLimiter::Remove(TabDownloadState* state) {
  DCHECK(ContainsKey(state_map_, state->web_contents()));
  state_map_.erase(state->web_contents());
  delete state;
}

// static
DownloadRequestLimiter::TestingDelegate* DownloadRequestLimiter::delegate_ =
    NULL;
