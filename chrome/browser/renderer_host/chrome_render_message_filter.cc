// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_message_filter.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/guest_view/web_view/web_view_permission_helper.h"
#include "chrome/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

#if defined(ENABLE_TASK_MANAGER)
#include "chrome/browser/task_manager/task_manager.h"
#endif

#if defined(USE_TCMALLOC)
#include "chrome/browser/browser_about_handler.h"
#endif

using content::BrowserThread;
using blink::WebCache;

namespace {

const uint32 kFilteredMessageClasses[] = {
  ChromeMsgStart,
};

}  // namespace

ChromeRenderMessageFilter::ChromeRenderMessageFilter(
    int render_process_id,
    Profile* profile)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      profile_(profile),
      predictor_(profile_->GetNetworkPredictor()),
      cookie_settings_(CookieSettings::Factory::GetForProfile(profile)) {
}

ChromeRenderMessageFilter::~ChromeRenderMessageFilter() {
}

bool ChromeRenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DnsPrefetch, OnDnsPrefetch)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_Preconnect, OnPreconnect)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ResourceTypeStats,
                        OnResourceTypeStats)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_UpdatedCacheStats,
                        OnUpdatedCacheStats)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_V8HeapStats, OnV8HeapStats)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDOMStorage, OnAllowDOMStorage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChromeViewHostMsg_RequestFileSystemAccessSync,
        OnRequestFileSystemAccessSync)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RequestFileSystemAccessAsync,
                        OnRequestFileSystemAccessAsync)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowIndexedDB, OnAllowIndexedDB)
#if defined(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_IsCrashReportingEnabled,
                        OnIsCrashReportingEnabled)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeRenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  switch (message.type()) {
    case ChromeViewHostMsg_ResourceTypeStats::ID:
    case ChromeViewHostMsg_UpdatedCacheStats::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ChromeRenderMessageFilter::OnDnsPrefetch(
    const std::vector<std::string>& hostnames) {
  if (predictor_)
    predictor_->DnsPrefetchList(hostnames);
}

void ChromeRenderMessageFilter::OnPreconnect(const GURL& url) {
  if (predictor_)
    predictor_->PreconnectUrl(
        url, GURL(), chrome_browser_net::UrlInfo::MOUSE_OVER_MOTIVATED, 1);
}

void ChromeRenderMessageFilter::OnResourceTypeStats(
    const WebCache::ResourceTypeStats& stats) {
  HISTOGRAM_COUNTS("WebCoreCache.ImagesSizeKB",
                   static_cast<int>(stats.images.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.CSSStylesheetsSizeKB",
                   static_cast<int>(stats.cssStyleSheets.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.ScriptsSizeKB",
                   static_cast<int>(stats.scripts.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.XSLStylesheetsSizeKB",
                   static_cast<int>(stats.xslStyleSheets.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.FontsSizeKB",
                   static_cast<int>(stats.fonts.size / 1024));

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(ENABLE_TASK_MANAGER)
  TaskManager::GetInstance()->model()->NotifyResourceTypeStats(peer_pid(),
                                                               stats);
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ChromeRenderMessageFilter::OnUpdatedCacheStats(
    const WebCache::UsageStats& stats) {
  WebCacheManager::GetInstance()->ObserveStats(render_process_id_, stats);
}

void ChromeRenderMessageFilter::OnV8HeapStats(int v8_memory_allocated,
                                              int v8_memory_used) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeRenderMessageFilter::OnV8HeapStats, this,
                   v8_memory_allocated, v8_memory_used));
    return;
  }

  base::ProcessId renderer_id = peer_pid();

#if defined(ENABLE_TASK_MANAGER)
  TaskManager::GetInstance()->model()->NotifyV8HeapStats(
      renderer_id,
      static_cast<size_t>(v8_memory_allocated),
      static_cast<size_t>(v8_memory_used));
#endif  // defined(ENABLE_TASK_MANAGER)

  V8HeapStatsDetails details(v8_memory_allocated, v8_memory_used);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_RENDERER_V8_HEAP_STATS_COMPUTED,
      content::Source<const base::ProcessId>(&renderer_id),
      content::Details<const V8HeapStatsDetails>(&details));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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

#if defined(ENABLE_EXTENSIONS)
void ChromeRenderMessageFilter::FileSystemAccessedSyncOnUIThread(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool blocked_by_policy,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool allowed =
      cookie_settings_->IsSettingCookieAllowed(origin_url, top_origin_url);

#if defined(ENABLE_EXTENSIONS)
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

#if defined(ENABLE_EXTENSIONS)
void ChromeRenderMessageFilter::FileSystemAccessedOnUIThread(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool allowed,
    base::Callback<void(bool)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

#if defined(ENABLE_PLUGINS)
void ChromeRenderMessageFilter::OnIsCrashReportingEnabled(bool* enabled) {
  *enabled = ChromeMetricsServiceAccessor::IsCrashReportingEnabled();
}
#endif
