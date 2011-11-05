// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_message_filter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/metrics/histogram_synchronizer.h"
#include "chrome/browser/metrics/tracking_synchronizer.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/plugin_service.h"
#include "content/browser/plugin_service_filter.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/common/child_process_info.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/plugins/npapi/plugin_list.h"

#if defined(USE_TCMALLOC)
#include "chrome/browser/browser_about_handler.h"
#endif

#if defined(OS_WIN)
// These includes are only necessary for the PluginInfobarExperiment.
#include "chrome/common/attrition_experiments.h"
#include "chrome/installer/util/google_update_settings.h"
#endif

using content::BrowserThread;
using WebKit::WebCache;
using WebKit::WebSecurityOrigin;

namespace {
// Override the behavior of the security infobars for plugins. Only
// operational on windows and only for a small slice of the of the
// UMA opted-in population.
void PluginInfobarExperiment(ContentSetting* outdated_policy,
                             ContentSetting* authorize_policy) {
#if !defined(OS_WIN)
  return;
#else
 std::wstring client_value;
 if (!GoogleUpdateSettings::GetClient(&client_value))
   return;
 if (client_value == attrition_experiments::kPluginNoBlockNoOOD) {
   *authorize_policy = CONTENT_SETTING_ALLOW;
   *outdated_policy = CONTENT_SETTING_ALLOW;
 } else if (client_value == attrition_experiments::kPluginNoBlockDoOOD) {
   *authorize_policy = CONTENT_SETTING_ALLOW;
   *outdated_policy = CONTENT_SETTING_BLOCK;
 } else if (client_value == attrition_experiments::kPluginDoBlockNoOOD) {
   *authorize_policy = CONTENT_SETTING_ASK;
   *outdated_policy = CONTENT_SETTING_ALLOW;
 } else if (client_value == attrition_experiments::kPluginDoBlockDoOOD) {
   *authorize_policy = CONTENT_SETTING_ASK;
   *outdated_policy = CONTENT_SETTING_BLOCK;
 }
#endif
}
}  // namespace

ChromeRenderMessageFilter::ChromeRenderMessageFilter(
    int render_process_id,
    Profile* profile,
    net::URLRequestContextGetter* request_context)
    : render_process_id_(render_process_id),
      profile_(profile),
      request_context_(request_context),
      extension_info_map_(profile->GetExtensionInfoMap()),
      resource_context_(profile->GetResourceContext()),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  allow_outdated_plugins_.Init(prefs::kPluginsAllowOutdated,
                               profile_->GetPrefs(), NULL);
  allow_outdated_plugins_.MoveToThread(BrowserThread::IO);
  always_authorize_plugins_.Init(prefs::kPluginsAlwaysAuthorize,
                                 profile_->GetPrefs(), NULL);
  always_authorize_plugins_.MoveToThread(BrowserThread::IO);
  host_content_settings_map_ = profile->GetHostContentSettingsMap();
  cookie_settings_ = CookieSettings::GetForProfile(profile);
}

ChromeRenderMessageFilter::~ChromeRenderMessageFilter() {
}

bool ChromeRenderMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                  bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ChromeRenderMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChromeViewHostMsg_LaunchNaCl, OnLaunchNaCl)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DnsPrefetch, OnDnsPrefetch)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RendererHistograms,
                        OnRendererHistograms)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RendererTrackedData,
                        OnRendererTrackedData)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_IsTrackingEnabled,
                        OnIsTrackingEnabled)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ResourceTypeStats,
                        OnResourceTypeStats)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_UpdatedCacheStats,
                        OnUpdatedCacheStats)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FPS, OnFPS)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_V8HeapStats, OnV8HeapStats)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToExtension,
                        OnOpenChannelToExtension)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToTab, OnOpenChannelToTab)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ExtensionHostMsg_GetMessageBundle,
                                    OnGetExtensionMessageBundle)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddListener, OnExtensionAddListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveListener,
                        OnExtensionRemoveListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ExtensionIdle, OnExtensionIdle)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ExtensionEventAck, OnExtensionEventAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_CloseChannel, OnExtensionCloseChannel)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RequestForIOThread,
                        OnExtensionRequestForIOThread)
#if defined(USE_TCMALLOC)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RendererTcmalloc, OnRendererTcmalloc)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_WriteTcmallocHeapProfile_ACK,
                        OnWriteTcmallocHeapProfile)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_GetPluginPolicies,
                        OnGetPluginPolicies)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDOMStorage, OnAllowDOMStorage)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowFileSystem, OnAllowFileSystem)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowIndexedDB, OnAllowIndexedDB)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_GetPluginContentSetting,
                        OnGetPluginContentSetting)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChromeViewHostMsg_GetPluginInfo,
                                    OnGetPluginInfo)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CanTriggerClipboardRead,
                        OnCanTriggerClipboardRead)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CanTriggerClipboardWrite,
                        OnCanTriggerClipboardWrite)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if ((message.type() == ChromeViewHostMsg_GetCookies::ID ||
       message.type() == ChromeViewHostMsg_SetCookie::ID) &&
    AutomationResourceMessageFilter::ShouldFilterCookieMessages(
        render_process_id_, message.routing_id())) {
    // ChromeFrame then we need to get/set cookies from the external host.
    IPC_BEGIN_MESSAGE_MAP_EX(ChromeRenderMessageFilter, message,
                             *message_was_ok)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ChromeViewHostMsg_GetCookies,
                                      OnGetCookies)
      IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SetCookie, OnSetCookie)
    IPC_END_MESSAGE_MAP()
    handled = true;
  }

  return handled;
}

void ChromeRenderMessageFilter::OnDestruct() const {
  const_cast<ChromeRenderMessageFilter*>(this)->
      weak_ptr_factory_.DetachFromThread();
  const_cast<ChromeRenderMessageFilter*>(this)->
      weak_ptr_factory_.InvalidateWeakPtrs();

  // Destroy on the UI thread because we contain a PrefMember.
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void ChromeRenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  switch (message.type()) {
    case ChromeViewHostMsg_ResourceTypeStats::ID:
#if defined(USE_TCMALLOC)
    case ChromeViewHostMsg_RendererTcmalloc::ID:
#endif
    case ExtensionHostMsg_AddListener::ID:
    case ExtensionHostMsg_RemoveListener::ID:
    case ExtensionHostMsg_ExtensionIdle::ID:
    case ExtensionHostMsg_ExtensionEventAck::ID:
    case ExtensionHostMsg_CloseChannel::ID:
    case ChromeViewHostMsg_UpdatedCacheStats::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ChromeRenderMessageFilter::OnLaunchNaCl(
    const std::wstring& url, int channel_descriptor, IPC::Message* reply_msg) {
  NaClProcessHost* host = new NaClProcessHost(url);
  host->Launch(this, channel_descriptor, reply_msg);
}

void ChromeRenderMessageFilter::OnDnsPrefetch(
    const std::vector<std::string>& hostnames) {
  if (profile_->GetNetworkPredictor())
    profile_->GetNetworkPredictor()->DnsPrefetchList(hostnames);
}

void ChromeRenderMessageFilter::OnRendererHistograms(
    int sequence_number,
    const std::vector<std::string>& histograms) {
  HistogramSynchronizer::DeserializeHistogramList(sequence_number, histograms);
}

void ChromeRenderMessageFilter::OnRendererTrackedData(
    int sequence_number,
    const std::string& tracked_data) {
  // TODO(rtenneti): Add support for other process types.
  chrome_browser_metrics::TrackingSynchronizer::DeserializeTrackingList(
      sequence_number, tracked_data, ChildProcessInfo::RENDER_PROCESS);
}

void ChromeRenderMessageFilter::OnIsTrackingEnabled() {
  chrome_browser_metrics::TrackingSynchronizer::IsTrackingEnabled(
      render_process_id_);
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
  TaskManager::GetInstance()->model()->NotifyResourceTypeStats(
      base::GetProcId(peer_handle()), stats);
}

void ChromeRenderMessageFilter::OnUpdatedCacheStats(
    const WebCache::UsageStats& stats) {
  WebCacheManager::GetInstance()->ObserveStats(render_process_id_, stats);
}

void ChromeRenderMessageFilter::OnFPS(int routing_id, float fps) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &ChromeRenderMessageFilter::OnFPS,
            routing_id, fps));
    return;
  }
  TaskManager::GetInstance()->model()->NotifyFPS(
      base::GetProcId(peer_handle()), routing_id, fps);
}

void ChromeRenderMessageFilter::OnV8HeapStats(int v8_memory_allocated,
                                        int v8_memory_used) {
  TaskManager::GetInstance()->model()->NotifyV8HeapStats(
      base::GetProcId(peer_handle()),
      static_cast<size_t>(v8_memory_allocated),
      static_cast<size_t>(v8_memory_used));
}

void ChromeRenderMessageFilter::OnOpenChannelToExtension(
    int routing_id, const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name, int* port_id) {
  int port2_id;
  ExtensionMessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ChromeRenderMessageFilter::OpenChannelToExtensionOnUIThread,
          render_process_id_, routing_id, port2_id, source_extension_id,
          target_extension_id, channel_name));
}

void ChromeRenderMessageFilter::OpenChannelToExtensionOnUIThread(
    int source_process_id, int source_routing_id,
    int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_->GetExtensionMessageService()->OpenChannelToExtension(
      source_process_id, source_routing_id, receiver_port_id,
      source_extension_id, target_extension_id, channel_name);
}

void ChromeRenderMessageFilter::OnOpenChannelToTab(
    int routing_id, int tab_id, const std::string& extension_id,
    const std::string& channel_name, int* port_id) {
  int port2_id;
  ExtensionMessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ChromeRenderMessageFilter::OpenChannelToTabOnUIThread,
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
  profile_->GetExtensionMessageService()->OpenChannelToTab(
      source_process_id, source_routing_id, receiver_port_id,
      tab_id, extension_id, channel_name);
}

void ChromeRenderMessageFilter::OnGetExtensionMessageBundle(
    const std::string& extension_id, IPC::Message* reply_msg) {
  const Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);
  FilePath extension_path;
  std::string default_locale;
  if (extension) {
    extension_path = extension->path();
    default_locale = extension->default_locale();
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this,
          &ChromeRenderMessageFilter::OnGetExtensionMessageBundleOnFileThread,
          extension_path, extension_id, default_locale, reply_msg));
}

void ChromeRenderMessageFilter::OnGetExtensionMessageBundleOnFileThread(
    const FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<ExtensionMessageBundle::SubstitutionMap> dictionary_map(
      extension_file_util::LoadExtensionMessageBundleSubstitutionMap(
          extension_path,
          extension_id,
          default_locale));

  ExtensionHostMsg_GetMessageBundle::WriteReplyParams(
      reply_msg, *dictionary_map);
  Send(reply_msg);
}

void ChromeRenderMessageFilter::OnExtensionAddListener(
    const std::string& extension_id,
    const std::string& event_name) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process || !profile_->GetExtensionEventRouter())
    return;

  profile_->GetExtensionEventRouter()->AddEventListener(
      event_name, process, extension_id);
}

void ChromeRenderMessageFilter::OnExtensionRemoveListener(
    const std::string& extension_id,
    const std::string& event_name) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process || !profile_->GetExtensionEventRouter())
    return;

  profile_->GetExtensionEventRouter()->RemoveEventListener(
      event_name, process, extension_id);
}

void ChromeRenderMessageFilter::OnExtensionIdle(
    const std::string& extension_id) {
  if (profile_->GetExtensionProcessManager())
    profile_->GetExtensionProcessManager()->OnExtensionIdle(extension_id);
}

void ChromeRenderMessageFilter::OnExtensionEventAck(
    const std::string& extension_id) {
  if (profile_->GetExtensionEventRouter())
    profile_->GetExtensionEventRouter()->OnExtensionEventAck(extension_id);
}

void ChromeRenderMessageFilter::OnExtensionCloseChannel(int port_id) {
  if (!RenderProcessHost::FromID(render_process_id_))
    return;  // To guard against crash in browser_tests shutdown.

  if (profile_->GetExtensionMessageService())
    profile_->GetExtensionMessageService()->CloseChannel(port_id);
}

void ChromeRenderMessageFilter::OnExtensionRequestForIOThread(
    int routing_id,
    const ExtensionHostMsg_Request_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ExtensionFunctionDispatcher::DispatchOnIOThread(
      extension_info_map_, profile_, render_process_id_,
      weak_ptr_factory_.GetWeakPtr(), routing_id, params);
}

#if defined(USE_TCMALLOC)
void ChromeRenderMessageFilter::OnRendererTcmalloc(const std::string& output) {
  base::ProcessId pid = base::GetProcId(peer_handle());
  AboutTcmallocRendererCallback(pid, output);
}

void ChromeRenderMessageFilter::OnWriteTcmallocHeapProfile(
    const FilePath::StringType& filepath,
    const std::string& output) {
  VLOG(0) << "Writing renderer heap profile dump to: " << filepath;
  file_util::WriteFile(FilePath(filepath), output.c_str(), output.size());
}
#endif

void ChromeRenderMessageFilter::OnGetPluginPolicies(
    ContentSetting* outdated_policy,
    ContentSetting* authorize_policy) {
  if (allow_outdated_plugins_.GetValue()) {
    *outdated_policy = CONTENT_SETTING_ALLOW;
  } else if (allow_outdated_plugins_.IsManaged()) {
    *outdated_policy = CONTENT_SETTING_BLOCK;
  } else {
    *outdated_policy = CONTENT_SETTING_ASK;
  }

  *authorize_policy = always_authorize_plugins_.GetValue() ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_ASK;

  PluginInfobarExperiment(outdated_policy, authorize_policy);
}

void ChromeRenderMessageFilter::OnAllowDatabase(int render_view_id,
                                                const GURL& origin_url,
                                                const GURL& top_origin_url,
                                                const string16& name,
                                                const string16& display_name,
                                                bool* allowed) {
  *allowed = cookie_settings_->IsSettingCookieAllowed(origin_url,
                                                      top_origin_url);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &TabSpecificContentSettings::WebDatabaseAccessed,
          render_process_id_, render_view_id, origin_url, name, display_name,
          !*allowed));
}

void ChromeRenderMessageFilter::OnAllowDOMStorage(int render_view_id,
                                                  const GURL& origin_url,
                                                  const GURL& top_origin_url,
                                                  DOMStorageType type,
                                                  bool* allowed) {
  *allowed = cookie_settings_->IsSettingCookieAllowed(origin_url,
                                                      top_origin_url);
  // Record access to DOM storage for potential display in UI.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &TabSpecificContentSettings::DOMStorageAccessed,
          render_process_id_, render_view_id, origin_url, type, !*allowed));
}

void ChromeRenderMessageFilter::OnAllowFileSystem(int render_view_id,
                                                  const GURL& origin_url,
                                                  const GURL& top_origin_url,
                                                  bool* allowed) {
  *allowed = cookie_settings_->IsSettingCookieAllowed(origin_url,
                                                      top_origin_url);
  // Record access to file system for potential display in UI.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &TabSpecificContentSettings::FileSystemAccessed,
          render_process_id_, render_view_id, origin_url, !*allowed));
}

void ChromeRenderMessageFilter::OnAllowIndexedDB(int render_view_id,
                                                 const GURL& origin_url,
                                                 const GURL& top_origin_url,
                                                 const string16& name,
                                                 bool* allowed) {
  *allowed = cookie_settings_->IsSettingCookieAllowed(origin_url,
                                                      top_origin_url);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &TabSpecificContentSettings::IndexedDBAccessed,
          render_process_id_, render_view_id, origin_url, name, !*allowed));
}

void ChromeRenderMessageFilter::OnGetPluginContentSetting(
    const GURL& policy_url,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern) {
  scoped_ptr<base::Value> value(
      host_content_settings_map_->GetContentSettingValue(
          policy_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS, resource,
          primary_pattern, secondary_pattern));
  if (!value.get()) {
    value.reset(host_content_settings_map_->GetContentSettingValue(
        policy_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
        primary_pattern, secondary_pattern));
  }
  *setting = content_settings::ValueToContentSetting(value.get());
}

struct ChromeRenderMessageFilter::GetPluginInfo_Params {
  int render_view_id;
  GURL url;
  GURL top_origin_url;
  std::string mime_type;
};

void ChromeRenderMessageFilter::OnGetPluginInfo(
    int render_view_id,
    const GURL& url,
    const GURL& top_origin_url,
    const std::string& mime_type,
    IPC::Message* reply_msg) {
  GetPluginInfo_Params params = {
    render_view_id,
    url,
    top_origin_url,
    mime_type
  };
  PluginService::GetInstance()->GetPlugins(
      base::Bind(&ChromeRenderMessageFilter::PluginsLoaded, this,
                 params, reply_msg));
}

void ChromeRenderMessageFilter::PluginsLoaded(
    const GetPluginInfo_Params& params,
    IPC::Message* reply_msg,
    const std::vector<webkit::WebPluginInfo>& plugins) {
  ChromeViewHostMsg_GetPluginInfo_Status status;
  webkit::WebPluginInfo plugin;
  std::string actual_mime_type;
  GetPluginInfo(params.render_view_id, params.url, params.top_origin_url,
                params.mime_type, &status, &plugin, &actual_mime_type);
  ChromeViewHostMsg_GetPluginInfo::WriteReplyParams(
      reply_msg, status, plugin, actual_mime_type);
  Send(reply_msg);
}

void ChromeRenderMessageFilter::GetPluginInfo(
    int render_view_id,
    const GURL& url,
    const GURL& top_origin_url,
    const std::string& mime_type,
    ChromeViewHostMsg_GetPluginInfo_Status* status,
    webkit::WebPluginInfo* plugin,
    std::string* actual_mime_type) {
  bool allow_wildcard = true;
  std::vector<webkit::WebPluginInfo> matching_plugins;
  std::vector<std::string> mime_types;
  PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, allow_wildcard, &matching_plugins, &mime_types);
  if (matching_plugins.empty()) {
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kNotFound;
    return;
  }

  if (matching_plugins.size() > 1 &&
      matching_plugins.back().path ==
          FilePath(webkit::npapi::kDefaultPluginLibraryName)) {
    // If there is at least one plug-in handling the required MIME type (apart
    // from the default plug-in), we don't need the default plug-in.
    matching_plugins.pop_back();
  }

  content::PluginServiceFilter* filter = PluginService::GetInstance()->filter();
  bool allowed = false;
  for (size_t i = 0; i < matching_plugins.size(); ++i) {
    if (!filter || filter->ShouldUsePlugin(render_process_id_,
                                           render_view_id,
                                           &resource_context_,
                                           url,
                                           top_origin_url,
                                           &matching_plugins[i])) {
      *plugin = matching_plugins[i];
      *actual_mime_type = mime_types[i];
      allowed = true;
      break;
    } else if (i == 0) {
      *plugin = matching_plugins[i];
      *actual_mime_type = mime_types[i];
    }
  }

  if (!allowed) {
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kDisabled;
    return;
  }

  status->value = ChromeViewHostMsg_GetPluginInfo_Status::kAllowed;
}

void ChromeRenderMessageFilter::OnCanTriggerClipboardRead(
    const GURL& origin, bool* allowed) {
  *allowed = extension_info_map_->SecurityOriginHasAPIPermission(
      origin, render_process_id_, ExtensionAPIPermission::kClipboardRead);
}

void ChromeRenderMessageFilter::OnCanTriggerClipboardWrite(
    const GURL& origin, bool* allowed) {
  // Since all extensions could historically write to the clipboard, preserve it
  // for compatibility.
  *allowed = (origin.SchemeIs(chrome::kExtensionScheme) ||
      extension_info_map_->SecurityOriginHasAPIPermission(
          origin, render_process_id_, ExtensionAPIPermission::kClipboardWrite));
}

void ChromeRenderMessageFilter::OnGetCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg) {
  AutomationResourceMessageFilter::GetCookiesForUrl(
      this, request_context_->GetURLRequestContext(), render_process_id_,
      reply_msg, url);
}

void ChromeRenderMessageFilter::OnSetCookie(const IPC::Message& message,
                                            const GURL& url,
                                            const GURL& first_party_for_cookies,
                                            const std::string& cookie) {
  AutomationResourceMessageFilter::SetCookiesForUrl(
      render_process_id_, message.routing_id(), url, cookie);
}
