// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_limiter.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/download/download_request_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;

// TabDownloadState ------------------------------------------------------------

DownloadRequestLimiter::TabDownloadState::TabDownloadState(
    DownloadRequestLimiter* host,
    content::WebContents* contents,
    content::WebContents* originating_web_contents)
    : content::WebContentsObserver(contents),
      web_contents_(contents),
      host_(host),
      status_(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD),
      download_count_(0),
      factory_(this) {
  content::Source<NavigationController> notification_source(
      &contents->GetController());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                 notification_source);
  NavigationEntry* active_entry = originating_web_contents ?
      originating_web_contents->GetController().GetActiveEntry() :
      contents->GetController().GetActiveEntry();
  if (active_entry)
    initial_page_host_ = active_entry->GetURL().host();
}

DownloadRequestLimiter::TabDownloadState::~TabDownloadState() {
  // We should only be destroyed after the callbacks have been notified.
  DCHECK(callbacks_.empty());

  // And we should have invalidated the back pointer.
  DCHECK(!factory_.HasWeakPtrs());
}

void DownloadRequestLimiter::TabDownloadState::AboutToNavigateRenderView(
    content::RenderViewHost* render_view_host) {
  switch (status_) {
    case ALLOW_ONE_DOWNLOAD:
    case PROMPT_BEFORE_DOWNLOAD:
      // When the user reloads the page without responding to the infobar, they
      // are expecting DownloadRequestLimiter to behave as if they had just
      // initially navigated to this page. See http://crbug.com/171372
      NotifyCallbacks(false);
      host_->Remove(this, web_contents());
      // WARNING: We've been deleted.
      break;
    case DOWNLOADS_NOT_ALLOWED:
    case ALLOW_ALL_DOWNLOADS:
      // Don't drop this information. The user has explicitly said that they
      // do/don't want downloads from this host.  If they accidentally Accepted
      // or Canceled, tough luck, they don't get another chance. They can copy
      // the URL into a new tab, which will make a new DownloadRequestLimiter.
      // See also the initial_page_host_ logic in Observe() for
      // NOTIFICATION_NAV_ENTRY_PENDING.
      break;
    default:
      NOTREACHED();
  }
}

void DownloadRequestLimiter::TabDownloadState::DidGetUserGesture() {
  if (is_showing_prompt()) {
    // Don't change the state if the user clicks on the page somewhere.
    return;
  }

  bool promptable = (InfoBarService::FromWebContents(web_contents()) != NULL);
  if (PermissionBubbleManager::Enabled()) {
    promptable =
        (PermissionBubbleManager::FromWebContents(web_contents()) != NULL);
  }

  // See PromptUserForDownload(): if there's no InfoBarService, then
  // DOWNLOADS_NOT_ALLOWED is functionally equivalent to PROMPT_BEFORE_DOWNLOAD.
  if ((status_ != DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS) &&
      (!promptable ||
       (status_ != DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED))) {
    // Revert to default status.
    host_->Remove(this, web_contents());
    // WARNING: We've been deleted.
  }
}

void DownloadRequestLimiter::TabDownloadState::WebContentsDestroyed() {
  // Tab closed, no need to handle closing the dialog as it's owned by the
  // WebContents.

  NotifyCallbacks(false);
  host_->Remove(this, web_contents());
  // WARNING: We've been deleted.
}

void DownloadRequestLimiter::TabDownloadState::PromptUserForDownload(
    const DownloadRequestLimiter::Callback& callback) {
  callbacks_.push_back(callback);
  DCHECK(web_contents_);
  if (is_showing_prompt())
    return;

  if (PermissionBubbleManager::Enabled()) {
    PermissionBubbleManager* bubble_manager =
        PermissionBubbleManager::FromWebContents(web_contents_);
    if (bubble_manager) {
      bubble_manager->AddRequest(new DownloadPermissionRequest(
          factory_.GetWeakPtr()));
    } else {
      Cancel();
    }
    return;
  }

  DownloadRequestInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents_), factory_.GetWeakPtr());
}

void DownloadRequestLimiter::TabDownloadState::SetContentSetting(
    ContentSetting setting) {
  if (!web_contents_)
    return;
  HostContentSettingsMap* settings =
    DownloadRequestLimiter::GetContentSettings(web_contents_);
  ContentSettingsPattern pattern(
      ContentSettingsPattern::FromURL(web_contents_->GetURL()));
  if (!settings || !pattern.IsValid())
    return;
  settings->SetContentSetting(
      pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
      std::string(),
      setting);
}

void DownloadRequestLimiter::TabDownloadState::Cancel() {
  SetContentSetting(CONTENT_SETTING_BLOCK);
  NotifyCallbacks(false);
}

void DownloadRequestLimiter::TabDownloadState::CancelOnce() {
  NotifyCallbacks(false);
}

void DownloadRequestLimiter::TabDownloadState::Accept() {
  SetContentSetting(CONTENT_SETTING_ALLOW);
  NotifyCallbacks(true);
}

DownloadRequestLimiter::TabDownloadState::TabDownloadState()
    : web_contents_(NULL),
      host_(NULL),
      status_(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD),
      download_count_(0),
      factory_(this) {
}

bool DownloadRequestLimiter::TabDownloadState::is_showing_prompt() const {
  return factory_.HasWeakPtrs();
}

void DownloadRequestLimiter::TabDownloadState::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_PENDING, type);
  content::NavigationController* controller = &web_contents()->GetController();
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
  if (ui::PageTransitionIsRedirect(entry->GetTransitionType()))
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

  NotifyCallbacks(false);
  host_->Remove(this, web_contents());
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
    // Null the generated weak pointer so we don't get notified again.
    factory_.InvalidateWeakPtrs();
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

HostContentSettingsMap* DownloadRequestLimiter::content_settings_ = NULL;

void DownloadRequestLimiter::SetContentSettingsForTesting(
    HostContentSettingsMap* content_settings) {
  content_settings_ = content_settings;
}

DownloadRequestLimiter::DownloadRequestLimiter()
    : factory_(this) {
}

DownloadRequestLimiter::~DownloadRequestLimiter() {
  // All the tabs should have closed before us, which sends notification and
  // removes from state_map_. As such, there should be no pending callbacks.
  DCHECK(state_map_.empty());
}

DownloadRequestLimiter::DownloadStatus
DownloadRequestLimiter::GetDownloadStatus(content::WebContents* web_contents) {
  TabDownloadState* state = GetDownloadState(web_contents, NULL, false);
  return state ? state->download_status() : ALLOW_ONE_DOWNLOAD;
}

void DownloadRequestLimiter::CanDownloadOnIOThread(
    int render_process_host_id,
    int render_view_id,
    const GURL& url,
    const std::string& request_method,
    const Callback& callback) {
  // This is invoked on the IO thread. Schedule the task to run on the UI
  // thread so that we can query UI state.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadRequestLimiter::CanDownload, this,
                 render_process_host_id, render_view_id, url,
                 request_method, callback));
}

DownloadRequestLimiter::TabDownloadState*
DownloadRequestLimiter::GetDownloadState(
    content::WebContents* web_contents,
    content::WebContents* originating_web_contents,
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
                                         const GURL& url,
                                         const std::string& request_method,
                                         const Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::WebContents* originating_contents =
      tab_util::GetWebContentsByID(render_process_host_id, render_view_id);
  if (!originating_contents) {
    // The WebContents was closed, don't allow the download.
    ScheduleNotification(callback, false);
    return;
  }

  if (!originating_contents->GetDelegate()) {
    ScheduleNotification(callback, false);
    return;
  }

  // Note that because |originating_contents| might go away before
  // OnCanDownloadDecided is invoked, we look it up by |render_process_host_id|
  // and |render_view_id|.
  base::Callback<void(bool)> can_download_callback = base::Bind(
      &DownloadRequestLimiter::OnCanDownloadDecided,
      factory_.GetWeakPtr(),
      render_process_host_id,
      render_view_id,
      request_method,
      callback);

  originating_contents->GetDelegate()->CanDownload(
      originating_contents->GetRenderViewHost(),
      url,
      request_method,
      can_download_callback);
}

void DownloadRequestLimiter::OnCanDownloadDecided(
    int render_process_host_id,
    int render_view_id,
    const std::string& request_method,
    const Callback& orig_callback, bool allow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::WebContents* originating_contents =
      tab_util::GetWebContentsByID(render_process_host_id, render_view_id);
  if (!originating_contents || !allow) {
    ScheduleNotification(orig_callback, false);
    return;
  }

  CanDownloadImpl(originating_contents,
                  request_method,
                  orig_callback);
}

HostContentSettingsMap* DownloadRequestLimiter::GetContentSettings(
    content::WebContents* contents) {
  return content_settings_ ? content_settings_ : Profile::FromBrowserContext(
      contents->GetBrowserContext())->GetHostContentSettingsMap();
}

void DownloadRequestLimiter::CanDownloadImpl(
    content::WebContents* originating_contents,
    const std::string& request_method,
    const Callback& callback) {
  DCHECK(originating_contents);

  TabDownloadState* state = GetDownloadState(
      originating_contents, originating_contents, true);
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
      state->increment_download_count();
      break;

    case DOWNLOADS_NOT_ALLOWED:
      ScheduleNotification(callback, false);
      break;

    case PROMPT_BEFORE_DOWNLOAD: {
      HostContentSettingsMap* content_settings = GetContentSettings(
          originating_contents);
      ContentSetting setting = CONTENT_SETTING_ASK;
      if (content_settings)
        setting = content_settings->GetContentSetting(
            originating_contents->GetURL(),
            originating_contents->GetURL(),
            CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
            std::string());
      switch (setting) {
        case CONTENT_SETTING_ALLOW: {
          TabSpecificContentSettings* settings =
              TabSpecificContentSettings::FromWebContents(
                  originating_contents);
          if (settings)
            settings->SetDownloadsBlocked(false);
          ScheduleNotification(callback, true);
          state->increment_download_count();
          return;
        }
        case CONTENT_SETTING_BLOCK: {
          TabSpecificContentSettings* settings =
              TabSpecificContentSettings::FromWebContents(
                  originating_contents);
          if (settings)
            settings->SetDownloadsBlocked(true);
          ScheduleNotification(callback, false);
          return;
        }
        case CONTENT_SETTING_DEFAULT:
        case CONTENT_SETTING_ASK:
        case CONTENT_SETTING_SESSION_ONLY:
          state->PromptUserForDownload(callback);
          state->increment_download_count();
          break;
        case CONTENT_SETTING_NUM_SETTINGS:
        default:
          NOTREACHED();
          return;
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

void DownloadRequestLimiter::ScheduleNotification(const Callback& callback,
                                                  bool allow) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, allow));
}

void DownloadRequestLimiter::Remove(TabDownloadState* state,
                                    content::WebContents* contents) {
  DCHECK(ContainsKey(state_map_, contents));
  state_map_.erase(contents);
  delete state;
}
