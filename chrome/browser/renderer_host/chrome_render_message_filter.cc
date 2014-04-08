// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_message_filter.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/api/messaging/message_service.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/file_util.h"
#include "extensions/common/message_bundle.h"

#if defined(USE_TCMALLOC)
#include "chrome/browser/browser_about_handler.h"
#endif

using content::BrowserThread;
using extensions::APIPermission;
using blink::WebCache;

namespace {

const uint32 kFilteredMessageClasses[] = {
  ChromeMsgStart,
  ExtensionMsgStart,
};

// Logs an action to the extension activity log for the specified profile.  Can
// be called from any thread.
void AddActionToExtensionActivityLog(
    Profile* profile,
    scoped_refptr<extensions::Action> action) {
#if defined(ENABLE_EXTENSIONS)
  // The ActivityLog can only be accessed from the main (UI) thread.  If we're
  // running on the wrong thread, re-dispatch from the main thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AddActionToExtensionActivityLog, profile, action));
  } else {
    if (!g_browser_process->profile_manager()->IsValidProfile(profile))
      return;
    // If the action included a URL, check whether it is for an incognito
    // profile.  The check is performed here so that it can safely be done from
    // the UI thread.
    if (action->page_url().is_valid() || !action->page_title().empty())
      action->set_page_incognito(profile->IsOffTheRecord());
    extensions::ActivityLog* activity_log =
        extensions::ActivityLog::GetInstance(profile);
    activity_log->LogAction(action);
  }
#endif
}

} // namespace

ChromeRenderMessageFilter::ChromeRenderMessageFilter(
    int render_process_id,
    Profile* profile,
    net::URLRequestContextGetter* request_context)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      profile_(profile),
      off_the_record_(profile_->IsOffTheRecord()),
      predictor_(profile_->GetNetworkPredictor()),
      request_context_(request_context),
      extension_info_map_(
          extensions::ExtensionSystem::Get(profile)->info_map()),
      cookie_settings_(CookieSettings::Factory::GetForProfile(profile)) {}

ChromeRenderMessageFilter::~ChromeRenderMessageFilter() {
}

bool ChromeRenderMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                  bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ChromeRenderMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DnsPrefetch, OnDnsPrefetch)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_Preconnect, OnPreconnect)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ResourceTypeStats,
                        OnResourceTypeStats)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_UpdatedCacheStats,
                        OnUpdatedCacheStats)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FPS, OnFPS)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_V8HeapStats, OnV8HeapStats)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToExtension,
                        OnOpenChannelToExtension)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToTab, OnOpenChannelToTab)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToNativeApp,
                        OnOpenChannelToNativeApp)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ExtensionHostMsg_GetMessageBundle,
                                    OnGetExtensionMessageBundle)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_CloseChannel, OnExtensionCloseChannel)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddAPIActionToActivityLog,
                        OnAddAPIActionToExtensionActivityLog);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddDOMActionToActivityLog,
                        OnAddDOMActionToExtensionActivityLog);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddEventToActivityLog,
                        OnAddEventToExtensionActivityLog);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDOMStorage, OnAllowDOMStorage)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowFileSystem, OnAllowFileSystem)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowIndexedDB, OnAllowIndexedDB)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CanTriggerClipboardRead,
                        OnCanTriggerClipboardRead)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CanTriggerClipboardWrite,
                        OnCanTriggerClipboardWrite)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_IsCrashReportingEnabled,
                        OnIsCrashReportingEnabled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeRenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  switch (message.type()) {
    case ChromeViewHostMsg_ResourceTypeStats::ID:
    case ExtensionHostMsg_CloseChannel::ID:
    case ChromeViewHostMsg_UpdatedCacheStats::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

net::HostResolver* ChromeRenderMessageFilter::GetHostResolver() {
  return request_context_->GetURLRequestContext()->host_resolver();
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

void ChromeRenderMessageFilter::OnFPS(int routing_id, float fps) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ChromeRenderMessageFilter::OnFPS, this,
            routing_id, fps));
    return;
  }

#if defined(ENABLE_TASK_MANAGER)
  TaskManager::GetInstance()->model()->NotifyFPS(
      peer_pid(), routing_id, fps);
#endif  // defined(ENABLE_TASK_MANAGER)
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

void ChromeRenderMessageFilter::OnOpenChannelToExtension(
    int routing_id,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& channel_name,
    bool include_tls_channel_id,
    int* port_id) {
  int port2_id;
  extensions::MessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromeRenderMessageFilter::OpenChannelToExtensionOnUIThread,
                 this, render_process_id_, routing_id, port2_id, info,
                 channel_name, include_tls_channel_id));
}

void ChromeRenderMessageFilter::OpenChannelToExtensionOnUIThread(
    int source_process_id, int source_routing_id,
    int receiver_port_id,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& channel_name,
    bool include_tls_channel_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extensions::MessageService::Get(profile_)->OpenChannelToExtension(
      source_process_id, source_routing_id, receiver_port_id,
      info.source_id, info.target_id, info.source_url, channel_name,
      include_tls_channel_id);
}

void ChromeRenderMessageFilter::OnOpenChannelToNativeApp(
    int routing_id,
    const std::string& source_extension_id,
    const std::string& native_app_name,
    int* port_id) {
  int port2_id;
  extensions::MessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromeRenderMessageFilter::OpenChannelToNativeAppOnUIThread,
                 this, routing_id, port2_id, source_extension_id,
                 native_app_name));
}

void ChromeRenderMessageFilter::OpenChannelToNativeAppOnUIThread(
    int source_routing_id,
    int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& native_app_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extensions::MessageService::Get(profile_)->OpenChannelToNativeApp(
      render_process_id_, source_routing_id, receiver_port_id,
      source_extension_id, native_app_name);
}

void ChromeRenderMessageFilter::OnOpenChannelToTab(
    int routing_id, int tab_id, const std::string& extension_id,
    const std::string& channel_name, int* port_id) {
  int port2_id;
  extensions::MessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromeRenderMessageFilter::OpenChannelToTabOnUIThread, this,
                 render_process_id_, routing_id, port2_id, tab_id, extension_id,
                 channel_name));
}

void ChromeRenderMessageFilter::OpenChannelToTabOnUIThread(
    int source_process_id, int source_routing_id,
    int receiver_port_id,
    int tab_id,
    const std::string& extension_id,
    const std::string& channel_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extensions::MessageService::Get(profile_)->OpenChannelToTab(
      source_process_id, source_routing_id, receiver_port_id,
      tab_id, extension_id, channel_name);
}

void ChromeRenderMessageFilter::OnGetExtensionMessageBundle(
    const std::string& extension_id, IPC::Message* reply_msg) {
  const extensions::Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);
  base::FilePath extension_path;
  std::string default_locale;
  if (extension) {
    extension_path = extension->path();
    default_locale = extensions::LocaleInfo::GetDefaultLocale(extension);
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &ChromeRenderMessageFilter::OnGetExtensionMessageBundleOnFileThread,
          this, extension_path, extension_id, default_locale, reply_msg));
}

void ChromeRenderMessageFilter::OnGetExtensionMessageBundleOnFileThread(
    const base::FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<extensions::MessageBundle::SubstitutionMap> dictionary_map(
      extensions::file_util::LoadMessageBundleSubstitutionMap(
          extension_path, extension_id, default_locale));

  ExtensionHostMsg_GetMessageBundle::WriteReplyParams(reply_msg,
                                                      *dictionary_map);
  Send(reply_msg);
}

void ChromeRenderMessageFilter::OnExtensionCloseChannel(
    int port_id,
    const std::string& error_message) {
  if (!content::RenderProcessHost::FromID(render_process_id_))
    return;  // To guard against crash in browser_tests shutdown.

  extensions::MessageService* message_service =
      extensions::MessageService::Get(profile_);
  if (message_service)
    message_service->CloseChannel(port_id, error_message);
}

void ChromeRenderMessageFilter::OnAddAPIActionToExtensionActivityLog(
    const std::string& extension_id,
    const ExtensionHostMsg_APIActionOrEvent_Params& params) {
  scoped_refptr<extensions::Action> action = new extensions::Action(
      extension_id, base::Time::Now(), extensions::Action::ACTION_API_CALL,
      params.api_call);
  action->set_args(make_scoped_ptr(params.arguments.DeepCopy()));
  if (!params.extra.empty()) {
    action->mutable_other()->SetString(
        activity_log_constants::kActionExtra, params.extra);
  }
  AddActionToExtensionActivityLog(profile_, action);
}

void ChromeRenderMessageFilter::OnAddDOMActionToExtensionActivityLog(
    const std::string& extension_id,
    const ExtensionHostMsg_DOMAction_Params& params) {
  scoped_refptr<extensions::Action> action = new extensions::Action(
      extension_id, base::Time::Now(), extensions::Action::ACTION_DOM_ACCESS,
      params.api_call);
  action->set_args(make_scoped_ptr(params.arguments.DeepCopy()));
  action->set_page_url(params.url);
  action->set_page_title(base::UTF16ToUTF8(params.url_title));
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      params.call_type);
  AddActionToExtensionActivityLog(profile_, action);
}

void ChromeRenderMessageFilter::OnAddEventToExtensionActivityLog(
    const std::string& extension_id,
    const ExtensionHostMsg_APIActionOrEvent_Params& params) {
  scoped_refptr<extensions::Action> action = new extensions::Action(
      extension_id, base::Time::Now(), extensions::Action::ACTION_API_EVENT,
      params.api_call);
  action->set_args(make_scoped_ptr(params.arguments.DeepCopy()));
  if (!params.extra.empty()) {
    action->mutable_other()->SetString(activity_log_constants::kActionExtra,
                                       params.extra);
  }
  AddActionToExtensionActivityLog(profile_, action);
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

void ChromeRenderMessageFilter::OnAllowFileSystem(int render_frame_id,
                                                  const GURL& origin_url,
                                                  const GURL& top_origin_url,
                                                  bool* allowed) {
  *allowed =
      cookie_settings_->IsSettingCookieAllowed(origin_url, top_origin_url);
  // Record access to file system for potential display in UI.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::FileSystemAccessed,
                 render_process_id_, render_frame_id, origin_url, !*allowed));
}

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

void ChromeRenderMessageFilter::OnCanTriggerClipboardRead(
    const GURL& origin, bool* allowed) {
  *allowed = extension_info_map_->SecurityOriginHasAPIPermission(
      origin, render_process_id_, APIPermission::kClipboardRead);
}

void ChromeRenderMessageFilter::OnCanTriggerClipboardWrite(
    const GURL& origin, bool* allowed) {
  // Since all extensions could historically write to the clipboard, preserve it
  // for compatibility.
  *allowed = (origin.SchemeIs(extensions::kExtensionScheme) ||
      extension_info_map_->SecurityOriginHasAPIPermission(
          origin, render_process_id_, APIPermission::kClipboardWrite));
}

void ChromeRenderMessageFilter::OnIsCrashReportingEnabled(bool* enabled) {
  *enabled = MetricsServiceHelper::IsCrashReportingEnabled();
}
