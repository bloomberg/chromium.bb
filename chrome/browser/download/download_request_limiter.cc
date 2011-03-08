// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_limiter.h"

#include "base/stl_util-inl.h"
#include "chrome/browser/download/download_request_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/common/notification_source.h"

// TabDownloadState ------------------------------------------------------------

DownloadRequestLimiter::TabDownloadState::TabDownloadState(
    DownloadRequestLimiter* host,
    NavigationController* controller,
    NavigationController* originating_controller)
    : host_(host),
      controller_(controller),
      status_(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD),
      download_count_(0),
      infobar_(NULL) {
  Source<NavigationController> notification_source(controller);
  registrar_.Add(this, NotificationType::NAV_ENTRY_PENDING,
                 notification_source);
  registrar_.Add(this, NotificationType::TAB_CLOSED, notification_source);

  NavigationEntry* active_entry = originating_controller ?
      originating_controller->GetActiveEntry() : controller->GetActiveEntry();
  if (active_entry)
    initial_page_host_ = active_entry->url().host();
}

DownloadRequestLimiter::TabDownloadState::~TabDownloadState() {
  // We should only be destroyed after the callbacks have been notified.
  DCHECK(callbacks_.empty());

  // And we should have closed the infobar.
  DCHECK(!infobar_);
}

void DownloadRequestLimiter::TabDownloadState::OnUserGesture() {
  if (is_showing_prompt()) {
    // Don't change the state if the user clicks on the page some where.
    return;
  }

  if (status_ != DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS &&
      status_ != DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED) {
    // Revert to default status.
    host_->Remove(this);
    // WARNING: We've been deleted.
    return;
  }
}

void DownloadRequestLimiter::TabDownloadState::PromptUserForDownload(
    TabContents* tab,
    DownloadRequestLimiter::Callback* callback) {
  callbacks_.push_back(callback);

  if (is_showing_prompt())
    return;  // Already showing prompt.

  if (DownloadRequestLimiter::delegate_) {
    NotifyCallbacks(DownloadRequestLimiter::delegate_->ShouldAllowDownload());
  } else {
    infobar_ = new DownloadRequestInfoBarDelegate(tab, this);
    tab->AddInfoBar(infobar_);
  }
}

void DownloadRequestLimiter::TabDownloadState::Cancel() {
  NotifyCallbacks(false);
}

void DownloadRequestLimiter::TabDownloadState::Accept() {
  NotifyCallbacks(true);
}

void DownloadRequestLimiter::TabDownloadState::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if ((type != NotificationType::NAV_ENTRY_PENDING &&
       type != NotificationType::TAB_CLOSED) ||
      Source<NavigationController>(source).ptr() != controller_) {
    NOTREACHED();
    return;
  }

  switch (type.value) {
    case NotificationType::NAV_ENTRY_PENDING: {
      // NOTE: resetting state on a pending navigate isn't ideal. In particular
      // it is possible that queued up downloads for the page before the
      // pending navigate will be delivered to us after we process this
      // request. If this happens we may let a download through that we
      // shouldn't have. But this is rather rare, and it is difficult to get
      // 100% right, so we don't deal with it.
      NavigationEntry* entry = controller_->pending_entry();
      if (!entry)
        return;

      if (PageTransition::IsRedirect(entry->transition_type())) {
        // Redirects don't count.
        return;
      }

      if (status_ == DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS ||
          status_ == DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED) {
        // User has either allowed all downloads or canceled all downloads. Only
        // reset the download state if the user is navigating to a different
        // host (or host is empty).
        if (!initial_page_host_.empty() && !entry->url().host().empty() &&
            entry->url().host() == initial_page_host_) {
          return;
        }
      }
      break;
    }

    case NotificationType::TAB_CLOSED:
      // Tab closed, no need to handle closing the dialog as it's owned by the
      // TabContents, break so that we get deleted after switch.
      break;

    default:
      NOTREACHED();
  }

  NotifyCallbacks(false);
  host_->Remove(this);
}

void DownloadRequestLimiter::TabDownloadState::NotifyCallbacks(bool allow) {
  status_ = allow ?
      DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS :
      DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED;
  std::vector<DownloadRequestLimiter::Callback*> callbacks;
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
    std::vector<DownloadRequestLimiter::Callback*>::iterator start, end;
    start = callbacks_.begin();
    end = callbacks_.begin() + kMaxDownloadsAtOnce;
    callbacks.assign(start, end);
    callbacks_.erase(start, end);
    change_status = true;
  }

  for (size_t i = 0; i < callbacks.size(); ++i)
    host_->ScheduleNotification(callbacks[i], allow);

  if (change_status)
    status_ = DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD;
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
    DownloadRequestLimiter::GetDownloadStatus(TabContents* tab) {
  TabDownloadState* state = GetDownloadState(&tab->controller(), NULL, false);
  return state ? state->download_status() : ALLOW_ONE_DOWNLOAD;
}

void DownloadRequestLimiter::CanDownloadOnIOThread(int render_process_host_id,
                                                   int render_view_id,
                                                   Callback* callback) {
  // This is invoked on the IO thread. Schedule the task to run on the UI
  // thread so that we can query UI state.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &DownloadRequestLimiter::CanDownload,
                        render_process_host_id, render_view_id, callback));
}

void DownloadRequestLimiter::OnUserGesture(TabContents* tab) {
  TabDownloadState* state = GetDownloadState(&tab->controller(), NULL, false);
  if (!state)
    return;

  state->OnUserGesture();
}

// static
void DownloadRequestLimiter::SetTestingDelegate(TestingDelegate* delegate) {
  delegate_ = delegate;
}

DownloadRequestLimiter::TabDownloadState* DownloadRequestLimiter::
    GetDownloadState(NavigationController* controller,
                     NavigationController* originating_controller,
                     bool create) {
  DCHECK(controller);
  StateMap::iterator i = state_map_.find(controller);
  if (i != state_map_.end())
    return i->second;

  if (!create)
    return NULL;

  TabDownloadState* state =
      new TabDownloadState(this, controller, originating_controller);
  state_map_[controller] = state;
  return state;
}

void DownloadRequestLimiter::CanDownload(int render_process_host_id,
                                         int render_view_id,
                                         Callback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TabContents* originating_tab =
      tab_util::GetTabContentsByID(render_process_host_id, render_view_id);
  if (!originating_tab) {
    // The tab was closed, don't allow the download.
    ScheduleNotification(callback, false);
    return;
  }
  CanDownloadImpl(originating_tab, callback);
}

void DownloadRequestLimiter::CanDownloadImpl(
    TabContents* originating_tab,
    Callback* callback) {
  // FYI: Chrome Frame overrides CanDownload in ExternalTabContainer in order
  // to cancel the download operation in chrome and let the host browser
  // take care of it.
  if (!originating_tab->CanDownload(callback->GetRequestId())) {
    ScheduleNotification(callback, false);
    return;
  }

  // If the tab requesting the download is a constrained popup that is not
  // shown, treat the request as if it came from the parent.
  TabContents* effective_tab = originating_tab;
  if (effective_tab->delegate()) {
    effective_tab =
        effective_tab->delegate()->GetConstrainingContents(effective_tab);
  }

  TabDownloadState* state = GetDownloadState(
      &effective_tab->controller(), &originating_tab->controller(), true);
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
      state->PromptUserForDownload(effective_tab, callback);
      state->increment_download_count();
      break;

    default:
      NOTREACHED();
  }
}

void DownloadRequestLimiter::ScheduleNotification(Callback* callback,
                                                  bool allow) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &DownloadRequestLimiter::NotifyCallback, callback, allow));
}

void DownloadRequestLimiter::NotifyCallback(Callback* callback, bool allow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (allow)
    callback->ContinueDownload();
  else
    callback->CancelDownload();
}

void DownloadRequestLimiter::Remove(TabDownloadState* state) {
  DCHECK(ContainsKey(state_map_, state->controller()));
  state_map_.erase(state->controller());
  delete state;
}

// static
DownloadRequestLimiter::TestingDelegate* DownloadRequestLimiter::delegate_ =
    NULL;
