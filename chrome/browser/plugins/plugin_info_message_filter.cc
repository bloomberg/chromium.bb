// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_info_message_filter.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "url/gurl.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif

using content::PluginService;
using content::WebPluginInfo;

namespace {

// For certain sandboxed Pepper plugins, use the JavaScript Content Settings.
bool ShouldUseJavaScriptSettingForPlugin(const WebPluginInfo& plugin) {
  if (plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS) {
    return false;
  }

  // Treat Native Client invocations like JavaScript.
  if (plugin.name == base::ASCIIToUTF16(ChromeContentClient::kNaClPluginName))
    return true;

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)
  // Treat CDM invocations like JavaScript.
  if (plugin.name == base::ASCIIToUTF16(kWidevineCdmDisplayName)) {
    DCHECK(plugin.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS);
    return true;
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)

  return false;
}

#if defined(ENABLE_PEPPER_CDMS)

enum PluginAvailabilityStatusForUMA {
  PLUGIN_NOT_REGISTERED,
  PLUGIN_AVAILABLE,
  PLUGIN_DISABLED,
  PLUGIN_AVAILABILITY_STATUS_MAX
};

static void SendPluginAvailabilityUMA(const std::string& mime_type,
                                      PluginAvailabilityStatusForUMA status) {
#if defined(WIDEVINE_CDM_AVAILABLE)
  // Only report results for Widevine CDM.
  if (mime_type != kWidevineCdmPluginMimeType)
    return;

  UMA_HISTOGRAM_ENUMERATION("Plugin.AvailabilityStatus.WidevineCdm",
                            status, PLUGIN_AVAILABILITY_STATUS_MAX);
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
}

#endif  // defined(ENABLE_PEPPER_CDMS)

}  // namespace

PluginInfoMessageFilter::Context::Context(int render_process_id,
                                          Profile* profile)
    : render_process_id_(render_process_id),
      resource_context_(profile->GetResourceContext()),
      host_content_settings_map_(profile->GetHostContentSettingsMap()),
      plugin_prefs_(PluginPrefs::GetForProfile(profile)) {
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

PluginInfoMessageFilter::Context::~Context() {
}

PluginInfoMessageFilter::PluginInfoMessageFilter(
    int render_process_id,
    Profile* profile)
    : BrowserMessageFilter(ChromeMsgStart),
      context_(render_process_id, profile),
      weak_ptr_factory_(this) {
}

bool PluginInfoMessageFilter::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PluginInfoMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChromeViewHostMsg_GetPluginInfo,
                                    OnGetPluginInfo)
#if defined(ENABLE_PEPPER_CDMS)
    IPC_MESSAGE_HANDLER(
        ChromeViewHostMsg_IsInternalPluginAvailableForMimeType,
        OnIsInternalPluginAvailableForMimeType)
#endif
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()
  return true;
}

void PluginInfoMessageFilter::OnDestruct() const {
  const_cast<PluginInfoMessageFilter*>(this)->
      weak_ptr_factory_.InvalidateWeakPtrs();

  // Destroy on the UI thread because we contain a |PrefMember|.
  content::BrowserThread::DeleteOnUIThread::Destruct(this);
}

PluginInfoMessageFilter::~PluginInfoMessageFilter() {}

struct PluginInfoMessageFilter::GetPluginInfo_Params {
  int render_frame_id;
  GURL url;
  GURL top_origin_url;
  std::string mime_type;
};

void PluginInfoMessageFilter::OnGetPluginInfo(
    int render_frame_id,
    const GURL& url,
    const GURL& top_origin_url,
    const std::string& mime_type,
    IPC::Message* reply_msg) {
  GetPluginInfo_Params params = {
    render_frame_id,
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
  if (context_.FindEnabledPlugin(params.render_frame_id, params.url,
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

#if defined(ENABLE_PEPPER_CDMS)

void PluginInfoMessageFilter::OnIsInternalPluginAvailableForMimeType(
    const std::string& mime_type,
    bool* is_available,
    std::vector<base::string16>* additional_param_names,
    std::vector<base::string16>* additional_param_values) {
  std::vector<WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);

  bool is_plugin_disabled = false;
  for (size_t i = 0; i < plugins.size(); ++i) {
    const WebPluginInfo& plugin = plugins[i];
    const std::vector<content::WebPluginMimeType>& mime_types =
        plugin.mime_types;
    for (size_t j = 0; j < mime_types.size(); ++j) {
      if (mime_types[j].mime_type == mime_type) {
        if (!context_.IsPluginEnabled(plugin)) {
          is_plugin_disabled = true;
          break;
        }

        *is_available = true;
        *additional_param_names = mime_types[j].additional_param_names;
        *additional_param_values = mime_types[j].additional_param_values;
        SendPluginAvailabilityUMA(mime_type, PLUGIN_AVAILABLE);
        return;
      }
    }
  }

  *is_available = false;
  SendPluginAvailabilityUMA(
      mime_type, is_plugin_disabled ? PLUGIN_DISABLED : PLUGIN_NOT_REGISTERED);
}

#endif // defined(ENABLE_PEPPER_CDMS)

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
  if (plugin.type == WebPluginInfo::PLUGIN_TYPE_NPAPI) {
    CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    // NPAPI plugins are not supported inside <webview> guests.
#if defined(ENABLE_EXTENSIONS)
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(
        render_process_id_)) {
      status->value =
          ChromeViewHostMsg_GetPluginInfo_Status::kNPAPINotSupported;
      return;
    }
#endif
  }

  ContentSetting plugin_setting = CONTENT_SETTING_DEFAULT;
  bool uses_default_content_setting = true;
  bool is_managed = false;
  // Check plug-in content settings. The primary URL is the top origin URL and
  // the secondary URL is the plug-in URL.
  GetPluginContentSetting(plugin, params.top_origin_url, params.url,
                          plugin_metadata->identifier(), &plugin_setting,
                          &uses_default_content_setting, &is_managed);
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
  // Check if the plug-in or its group is enabled by policy.
  PluginPrefs::PolicyStatus plugin_policy =
      plugin_prefs_->PolicyStatusForPlugin(plugin.name);
  PluginPrefs::PolicyStatus group_policy =
      plugin_prefs_->PolicyStatusForPlugin(plugin_metadata->name());

  // Check if the plug-in requires authorization.
  if (plugin_status ==
          PluginMetadata::SECURITY_STATUS_REQUIRES_AUTHORIZATION &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS &&
      !always_authorize_plugins_.GetValue() &&
      plugin_setting != CONTENT_SETTING_BLOCK &&
      uses_default_content_setting &&
      plugin_policy != PluginPrefs::POLICY_ENABLED &&
      group_policy != PluginPrefs::POLICY_ENABLED &&
      !ChromePluginServiceFilter::GetInstance()->IsPluginRestricted(
          plugin.path)) {
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

  if (plugin_setting == CONTENT_SETTING_ASK) {
      status->value = ChromeViewHostMsg_GetPluginInfo_Status::kClickToPlay;
  } else if (plugin_setting == CONTENT_SETTING_BLOCK) {
    status->value =
        is_managed ? ChromeViewHostMsg_GetPluginInfo_Status::kBlockedByPolicy
                   : ChromeViewHostMsg_GetPluginInfo_Status::kBlocked;
  }

  if (status->value == ChromeViewHostMsg_GetPluginInfo_Status::kAllowed) {
    // Allow an embedder of <webview> to block a plugin from being loaded inside
    // the guest. In order to do this, set the status to 'Unauthorized' here,
    // and update the status as appropriate depending on the response from the
    // embedder.
#if defined(ENABLE_EXTENSIONS)
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(
        render_process_id_))
      status->value = ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized;

#endif
  }
}

bool PluginInfoMessageFilter::Context::FindEnabledPlugin(
    int render_frame_id,
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
                                             render_frame_id,
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
    bool* uses_default_content_setting,
    bool* is_managed) const {
  scoped_ptr<base::Value> value;
  content_settings::SettingInfo info;
  bool uses_plugin_specific_setting = false;
  if (ShouldUseJavaScriptSettingForPlugin(plugin)) {
    value.reset(
        host_content_settings_map_->GetWebsiteSetting(
            policy_url, policy_url, CONTENT_SETTINGS_TYPE_JAVASCRIPT,
            std::string(), &info));
  } else {
    content_settings::SettingInfo specific_info;
    scoped_ptr<base::Value> specific_setting(
        host_content_settings_map_->GetWebsiteSetting(
            policy_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS, resource,
            &specific_info));
    content_settings::SettingInfo general_info;
    scoped_ptr<base::Value> general_setting(
        host_content_settings_map_->GetWebsiteSetting(
            policy_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS,
            std::string(), &general_info));

    // If there is a plugin-specific setting, we use it, unless the general
    // setting was set by policy, in which case it takes precedence.
    uses_plugin_specific_setting = specific_setting &&
        (general_info.source != content_settings::SETTING_SOURCE_POLICY);
    if (uses_plugin_specific_setting) {
      value = specific_setting.Pass();
      info = specific_info;
    } else {
      value = general_setting.Pass();
      info = general_info;
    }
  }
  *setting = content_settings::ValueToContentSetting(value.get());
  *uses_default_content_setting =
      !uses_plugin_specific_setting &&
      info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard();
  *is_managed = info.source == content_settings::SETTING_SOURCE_POLICY;
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

bool PluginInfoMessageFilter::Context::IsPluginEnabled(
    const content::WebPluginInfo& plugin) const {
  return plugin_prefs_->IsPluginEnabled(plugin);
}
