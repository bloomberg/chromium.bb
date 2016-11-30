// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_message_filter.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/network_hints/common/network_hints_common.h"
#include "components/network_hints/common/network_hints_messages.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "extensions/features/features.h"
#include "ppapi/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#endif

using content::BrowserThread;

namespace {

const uint32_t kFilteredMessageClasses[] = {
    ChromeMsgStart, NetworkHintsMsgStart,
};

void DidStartServiceWorkerForNavigationHint(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

}  // namespace

ChromeRenderMessageFilter::ChromeRenderMessageFilter(
    int render_process_id,
    Profile* profile,
    content::ServiceWorkerContext* service_worker_context)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      profile_(profile),
      predictor_(profile_->GetNetworkPredictor()),
      cookie_settings_(CookieSettingsFactory::GetForProfile(profile)),
      service_worker_context_(service_worker_context) {}

ChromeRenderMessageFilter::~ChromeRenderMessageFilter() {
}

bool ChromeRenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(NetworkHintsMsg_DNSPrefetch, OnDnsPrefetch)
    IPC_MESSAGE_HANDLER(NetworkHintsMsg_Preconnect, OnPreconnect)
    IPC_MESSAGE_HANDLER(NetworkHintsMsg_NavigationHint, OnNavigationHint)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_UpdatedCacheStats,
                        OnUpdatedCacheStats)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDOMStorage, OnAllowDOMStorage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChromeViewHostMsg_RequestFileSystemAccessSync,
        OnRequestFileSystemAccessSync)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RequestFileSystemAccessAsync,
                        OnRequestFileSystemAccessAsync)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowIndexedDB, OnAllowIndexedDB)
#if BUILDFLAG(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_IsCrashReportingEnabled,
                        OnIsCrashReportingEnabled)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FieldTrialActivated,
                        OnFieldTrialActivated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeRenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  switch (message.type()) {
    case NetworkHintsMsg_NavigationHint::ID:
#if BUILDFLAG(ENABLE_PLUGINS)
    case ChromeViewHostMsg_IsCrashReportingEnabled::ID:
#endif
    case ChromeViewHostMsg_UpdatedCacheStats::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ChromeRenderMessageFilter::OnDnsPrefetch(
    const network_hints::LookupRequest& request) {
  if (predictor_)
    predictor_->DnsPrefetchList(request.hostname_list);
}

void ChromeRenderMessageFilter::OnPreconnect(const GURL& url,
                                             bool allow_credentials,
                                             int count) {
  if (count < 1) {
    LOG(WARNING) << "NetworkHintsMsg_Preconnect IPC with invalid count: "
                 << count;
    return;
  }
  if (predictor_ && url.is_valid() && url.has_host() && url.has_scheme() &&
      url.SchemeIsHTTPOrHTTPS()) {
    predictor_->PreconnectUrl(url, GURL(),
                              chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
                              allow_credentials, count);
  }
}

void ChromeRenderMessageFilter::OnNavigationHint(
    const GURL& url,
    blink::WebNavigationHintType type) {
  // TODO(horo): We don't need to have this method in //chrome. Move it to
  // //content while mojoifing network_hints. (crbug.com/610750)
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service_worker_context_->StartServiceWorkerForNavigationHint(
      url, type, render_process_id_,
      base::Bind(&DidStartServiceWorkerForNavigationHint));
}

void ChromeRenderMessageFilter::OnUpdatedCacheStats(uint64_t capacity,
                                                    uint64_t size) {
  web_cache::WebCacheManager::GetInstance()->ObserveStats(render_process_id_,
                                                          capacity, size);
}

void ChromeRenderMessageFilter::OnAllowDatabase(
    int render_frame_id,
    const GURL& origin_url,
    const GURL& top_origin_url,
    const base::string16& name,
    const base::string16& display_name,
    bool* allowed) {
  *allowed =
      cookie_settings_->IsSettingCookieAllowed(origin_url, top_origin_url);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::WebDatabaseAccessed,
                 render_process_id_, render_frame_id, origin_url, name,
                 display_name, !*allowed));
}

void ChromeRenderMessageFilter::OnAllowDOMStorage(int render_frame_id,
                                                  const GURL& origin_url,
                                                  const GURL& top_origin_url,
                                                  bool local,
                                                  bool* allowed) {
  *allowed =
      cookie_settings_->IsSettingCookieAllowed(origin_url, top_origin_url);
  // Record access to DOM storage for potential display in UI.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::DOMStorageAccessed,
                 render_process_id_, render_frame_id, origin_url, local,
                 !*allowed));
}

void ChromeRenderMessageFilter::OnRequestFileSystemAccessSync(
    int render_frame_id,
    const GURL& origin_url,
    const GURL& top_origin_url,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::Callback<void(bool)> callback =
      base::Bind(&ChromeRenderMessageFilter::
                 OnRequestFileSystemAccessSyncResponse,
                 make_scoped_refptr(this),
                 reply_msg);
  OnRequestFileSystemAccess(render_frame_id,
                            origin_url,
                            top_origin_url,
                            callback);
}

void ChromeRenderMessageFilter::OnRequestFileSystemAccessSyncResponse(
    IPC::Message* reply_msg,
    bool allowed) {
  ChromeViewHostMsg_RequestFileSystemAccessSync::WriteReplyParams(reply_msg,
                                                                  allowed);
  Send(reply_msg);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeRenderMessageFilter::FileSystemAccessedSyncOnUIThread(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool blocked_by_policy,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromFrameID(
          render_process_id, render_frame_id);
  // Between the time the permission request is made and the time it is handled
  // by the UI thread, the extensions::WebViewPermissionHelper might be gone.
  if (!web_view_permission_helper)
    return;
  web_view_permission_helper->FileSystemAccessedSync(
      render_process_id, render_frame_id, url, blocked_by_policy, reply_msg);
}
#endif

void ChromeRenderMessageFilter::OnRequestFileSystemAccessAsync(
    int render_frame_id,
    int request_id,
    const GURL& origin_url,
    const GURL& top_origin_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::Callback<void(bool)> callback =
      base::Bind(&ChromeRenderMessageFilter::
                 OnRequestFileSystemAccessAsyncResponse,
                 make_scoped_refptr(this),
                 render_frame_id,
                 request_id);
  OnRequestFileSystemAccess(render_frame_id,
                            origin_url,
                            top_origin_url,
                            callback);
}

void ChromeRenderMessageFilter::OnRequestFileSystemAccessAsyncResponse(
    int render_frame_id,
    int request_id,
    bool allowed) {
  Send(new ChromeViewMsg_RequestFileSystemAccessAsyncResponse(
      render_frame_id, request_id, allowed));
}

void ChromeRenderMessageFilter::OnRequestFileSystemAccess(
    int render_frame_id,
    const GURL& origin_url,
    const GURL& top_origin_url,
    base::Callback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bool allowed =
      cookie_settings_->IsSettingCookieAllowed(origin_url, top_origin_url);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool is_web_view_guest = extensions::WebViewRendererState::GetInstance()
      ->IsGuest(render_process_id_);
  if (is_web_view_guest) {
    // Record access to file system for potential display in UI.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ChromeRenderMessageFilter::FileSystemAccessedOnUIThread,
                   render_process_id_,
                   render_frame_id,
                   origin_url,
                   allowed,
                   callback));
    return;
  }
#endif
  callback.Run(allowed);
  // Record access to file system for potential display in UI.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TabSpecificContentSettings::FileSystemAccessed,
                 render_process_id_,
                 render_frame_id,
                 origin_url,
                 !allowed));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeRenderMessageFilter::FileSystemAccessedOnUIThread(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool allowed,
    base::Callback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromFrameID(
          render_process_id, render_frame_id);
  // Between the time the permission request is made and the time it is handled
  // by the UI thread, the extensions::WebViewPermissionHelper might be gone.
  if (!web_view_permission_helper)
    return;
  web_view_permission_helper->RequestFileSystemPermission(
      url,
      allowed,
      base::Bind(&ChromeRenderMessageFilter::FileSystemAccessedResponse,
                 render_process_id,
                 render_frame_id,
                 url,
                 callback));
}

void ChromeRenderMessageFilter::FileSystemAccessedResponse(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    base::Callback<void(bool)> callback,
    bool allowed) {
  TabSpecificContentSettings::FileSystemAccessed(
      render_process_id, render_frame_id, url, !allowed);
  callback.Run(allowed);
}
#endif

void ChromeRenderMessageFilter::OnAllowIndexedDB(int render_frame_id,
                                                 const GURL& origin_url,
                                                 const GURL& top_origin_url,
                                                 const base::string16& name,
                                                 bool* allowed) {
  *allowed =
      cookie_settings_->IsSettingCookieAllowed(origin_url, top_origin_url);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::IndexedDBAccessed,
                 render_process_id_, render_frame_id, origin_url, name,
                 !*allowed));
}

#if BUILDFLAG(ENABLE_PLUGINS)
void ChromeRenderMessageFilter::OnIsCrashReportingEnabled(bool* enabled) {
  *enabled = ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}
#endif

void ChromeRenderMessageFilter::OnFieldTrialActivated(
    const std::string& trial_name) {
  // Activate the trial in the browser process to match its state in the
  // renderer. This is done by calling FindFullName which finalizes the group
  // and activates the trial.
  base::FieldTrialList::FindFullName(trial_name);
}
