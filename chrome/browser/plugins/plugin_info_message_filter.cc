// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_info_message_filter.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "googleurl/src/gurl.h"
#include "webkit/plugins/npapi/plugin_list.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif

using content::PluginService;
using webkit::WebPluginInfo;

namespace {

// For certain sandboxed Pepper plugins, use the JavaScript Content Settings.
bool ShouldUseJavaScriptSettingForPlugin(const WebPluginInfo& plugin) {
  if (plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS) {
    return false;
  }

  // Treat Native Client invocations like JavaScript.
  if (plugin.name == ASCIIToUTF16(chrome::ChromeContentClient::kNaClPluginName))
    return true;

#if defined(WIDEVINE_CDM_AVAILABLE)
  // Treat CDM invocations like JavaScript.
  if (plugin.name == ASCIIToUTF16(kWidevineCdmPluginName)) {
    DCHECK(plugin.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS);
    return true;
  }
#endif  // WIDEVINE_CDM_AVAILABLE

  return false;
}

}  // namespace

PluginInfoMessageFilter::Context::Context(int render_process_id,
                                          Profile* profile)
    : render_process_id_(render_process_id),
      resource_context_(profile->GetResourceContext()),
      host_content_settings_map_(profile->GetHostContentSettingsMap()) {
  allow_outdated_plugins_.Init(prefs::kPluginsAllowOutdated,
                               profile->GetPrefs());
  allow_outdated_plugins_.MoveToThread(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));
  always_authorize_plugins_.Init(prefs::kPluginsAlwaysAuthorize,
                                 profile->GetPrefs());
  always_authorize_plugins_.MoveToThread(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));
}

PluginInfoMessageFilter::Context::Context()
    : render_process_id_(0),
      resource_context_(NULL),
      host_content_settings_map_(NULL) {
}

PluginInfoMessageFilter::Context::~Context() {
}

PluginInfoMessageFilter::PluginInfoMessageFilter(
    int render_process_id,
    Profile* profile)
    : context_(render_process_id, profile),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

bool PluginInfoMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  IPC_BEGIN_MESSAGE_MAP_EX(PluginInfoMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChromeViewHostMsg_GetPluginInfo,
                                    OnGetPluginInfo)
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()
  return true;
}

void PluginInfoMessageFilter::OnDestruct() const {
  const_cast<PluginInfoMessageFilter*>(this)->
      weak_ptr_factory_.DetachFromThread();
  const_cast<PluginInfoMessageFilter*>(this)->
      weak_ptr_factory_.InvalidateWeakPtrs();

  // Destroy on the UI thread because we contain a |PrefMember|.
  content::BrowserThread::DeleteOnUIThread::Destruct(this);
}

PluginInfoMessageFilter::~PluginInfoMessageFilter() {}

struct PluginInfoMessageFilter::GetPluginInfo_Params {
  int render_view_id;
  GURL url;
  GURL top_origin_url;
  std::string mime_type;
};

void PluginInfoMessageFilter::OnGetPluginInfo(
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
      base::Bind(&PluginInfoMessageFilter::PluginsLoaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 params, reply_msg));
}

void PluginInfoMessageFilter::PluginsLoaded(
    const GetPluginInfo_Params& params,
    IPC::Message* reply_msg,
    const std::vector<WebPluginInfo>& plugins) {
  ChromeViewHostMsg_GetPluginInfo_Output output;
  // This also fills in |actual_mime_type|.
  scoped_ptr<PluginMetadata> plugin_metadata;
  if (context_.FindEnabledPlugin(params.render_view_id, params.url,
                                 params.top_origin_url, params.mime_type,
                                 &output.status, &output.plugin,
                                 &output.actual_mime_type,
                                 &plugin_metadata)) {
    context_.DecidePluginStatus(params, output.plugin, plugin_metadata.get(),
                                &output.status);
  }

  if (plugin_metadata) {
    output.group_identifier = plugin_metadata->identifier();
    output.group_name = plugin_metadata->name();
  }

  context_.MaybeGrantAccess(output.status, output.plugin.path);

  ChromeViewHostMsg_GetPluginInfo::WriteReplyParams(reply_msg, output);
  Send(reply_msg);
}

void PluginInfoMessageFilter::Context::DecidePluginStatus(
    const GetPluginInfo_Params& params,
    const WebPluginInfo& plugin,
    const PluginMetadata* plugin_metadata,
    ChromeViewHostMsg_GetPluginInfo_Status* status) const {
#if defined(OS_WIN)
  if (plugin.type == WebPluginInfo::PLUGIN_TYPE_NPAPI &&
      base::win::IsMetroProcess()) {
    status->value =
        ChromeViewHostMsg_GetPluginInfo_Status::kNPAPINotSupported;
    return;
  }
#endif

  ContentSetting plugin_setting = CONTENT_SETTING_DEFAULT;
  bool uses_default_content_setting = true;
  // Check plug-in content settings. The primary URL is the top origin URL and
  // the secondary URL is the plug-in URL.
  GetPluginContentSetting(plugin, params.top_origin_url, params.url,
                          plugin_metadata->identifier(), &plugin_setting,
                          &uses_default_content_setting);
  DCHECK(plugin_setting != CONTENT_SETTING_DEFAULT);

  PluginMetadata::SecurityStatus plugin_status =
      plugin_metadata->GetSecurityStatus(plugin);
#if defined(ENABLE_PLUGIN_INSTALLATION)
  // Check if the plug-in is outdated.
  if (plugin_status == PluginMetadata::SECURITY_STATUS_OUT_OF_DATE &&
      !allow_outdated_plugins_.GetValue()) {
    if (allow_outdated_plugins_.IsManaged()) {
      status->value =
          ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedDisallowed;
    } else {
      status->value = ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked;
    }
    return;
  }
#endif

  // Check if the plug-in requires authorization.
  if (plugin_status ==
          PluginMetadata::SECURITY_STATUS_REQUIRES_AUTHORIZATION &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS &&
      !always_authorize_plugins_.GetValue() &&
      plugin_setting != CONTENT_SETTING_BLOCK &&
      uses_default_content_setting) {
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized;
    return;
  }

  // Check if the plug-in is crashing too much.
  if (PluginService::GetInstance()->IsPluginUnstable(plugin.path) &&
      !always_authorize_plugins_.GetValue() &&
      plugin_setting != CONTENT_SETTING_BLOCK &&
      uses_default_content_setting) {
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized;
    return;
  }

  if (plugin_setting == CONTENT_SETTING_ASK)
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kClickToPlay;
  else if (plugin_setting == CONTENT_SETTING_BLOCK)
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kBlocked;
}

bool PluginInfoMessageFilter::Context::FindEnabledPlugin(
    int render_view_id,
    const GURL& url,
    const GURL& top_origin_url,
    const std::string& mime_type,
    ChromeViewHostMsg_GetPluginInfo_Status* status,
    WebPluginInfo* plugin,
    std::string* actual_mime_type,
    scoped_ptr<PluginMetadata>* plugin_metadata) const {
  bool allow_wildcard = true;
  std::vector<WebPluginInfo> matching_plugins;
  std::vector<std::string> mime_types;
  PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, allow_wildcard, &matching_plugins, &mime_types);
  if (matching_plugins.empty()) {
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kNotFound;
    return false;
  }

  content::PluginServiceFilter* filter =
      PluginService::GetInstance()->GetFilter();
  size_t i = 0;
  for (; i < matching_plugins.size(); ++i) {
    if (!filter || filter->IsPluginAvailable(render_process_id_,
                                             render_view_id,
                                             resource_context_,
                                             url,
                                             top_origin_url,
                                             &matching_plugins[i])) {
      break;
    }
  }

  // If we broke out of the loop, we have found an enabled plug-in.
  bool enabled = i < matching_plugins.size();
  if (!enabled) {
    // Otherwise, we only found disabled plug-ins, so we take the first one.
    i = 0;
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kDisabled;
  }

  *plugin = matching_plugins[i];
  *actual_mime_type = mime_types[i];
  if (plugin_metadata)
    *plugin_metadata = PluginFinder::GetInstance()->GetPluginMetadata(*plugin);

  return enabled;
}

void PluginInfoMessageFilter::Context::GetPluginContentSetting(
    const WebPluginInfo& plugin,
    const GURL& policy_url,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    bool* uses_default_content_setting) const {

  scoped_ptr<base::Value> value;
  content_settings::SettingInfo info;
  bool uses_plugin_specific_setting = false;
  if (ShouldUseJavaScriptSettingForPlugin(plugin)) {
    value.reset(
        host_content_settings_map_->GetWebsiteSetting(
            policy_url, policy_url, CONTENT_SETTINGS_TYPE_JAVASCRIPT,
            std::string(), &info));
  } else {
    value.reset(
        host_content_settings_map_->GetWebsiteSetting(
            policy_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS, resource,
            &info));
    if (value.get()) {
      uses_plugin_specific_setting = true;
    } else {
      value.reset(host_content_settings_map_->GetWebsiteSetting(
          policy_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
          &info));
    }
  }
  *setting = content_settings::ValueToContentSetting(value.get());
  *uses_default_content_setting =
      !uses_plugin_specific_setting &&
      info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard();
}

void PluginInfoMessageFilter::Context::MaybeGrantAccess(
    const ChromeViewHostMsg_GetPluginInfo_Status& status,
    const base::FilePath& path) const {
  if (status.value == ChromeViewHostMsg_GetPluginInfo_Status::kAllowed ||
      status.value == ChromeViewHostMsg_GetPluginInfo_Status::kClickToPlay) {
    ChromePluginServiceFilter::GetInstance()->AuthorizePlugin(
        render_process_id_, path);
  }
}

