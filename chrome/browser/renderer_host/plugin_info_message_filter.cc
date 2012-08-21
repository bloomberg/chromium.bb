// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/plugin_info_message_filter.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "googleurl/src/gurl.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugin_finder.h"
#include "chrome/browser/plugin_installer.h"
#endif

using content::PluginService;
using webkit::WebPluginInfo;

PluginInfoMessageFilter::Context::Context(int render_process_id,
                                          Profile* profile)
    : render_process_id_(render_process_id),
      resource_context_(profile->GetResourceContext()),
      host_content_settings_map_(profile->GetHostContentSettingsMap()) {
  allow_outdated_plugins_.Init(prefs::kPluginsAllowOutdated,
                               profile->GetPrefs(), NULL);
  allow_outdated_plugins_.MoveToThread(content::BrowserThread::IO);
  always_authorize_plugins_.Init(prefs::kPluginsAlwaysAuthorize,
                                 profile->GetPrefs(), NULL);
  always_authorize_plugins_.MoveToThread(content::BrowserThread::IO);
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
  ChromeViewHostMsg_GetPluginInfo_Status status;
  WebPluginInfo plugin;
  std::string actual_mime_type;
  // This also fills in |actual_mime_type|.
  if (!context_.FindEnabledPlugin(params.render_view_id, params.url,
                                  params.top_origin_url, params.mime_type,
                                  &status, &plugin, &actual_mime_type)) {
    ChromeViewHostMsg_GetPluginInfo::WriteReplyParams(
        reply_msg, status, plugin, actual_mime_type);
    Send(reply_msg);
    return;
  }
#if defined(ENABLE_PLUGIN_INSTALLATION)
  PluginFinder::Get(base::Bind(&PluginInfoMessageFilter::GotPluginFinder, this,
                               params, reply_msg, plugin, actual_mime_type));
#else
  GotPluginFinder(params, reply_msg, plugin, actual_mime_type, NULL);
#endif
}

void PluginInfoMessageFilter::GotPluginFinder(
    const GetPluginInfo_Params& params,
    IPC::Message* reply_msg,
    const WebPluginInfo& plugin,
    const std::string& actual_mime_type,
    PluginFinder* plugin_finder) {
  ChromeViewHostMsg_GetPluginInfo_Status status;
  context_.DecidePluginStatus(params, plugin, plugin_finder, &status);
  ChromeViewHostMsg_GetPluginInfo::WriteReplyParams(
      reply_msg, status, plugin, actual_mime_type);
  Send(reply_msg);
}

void PluginInfoMessageFilter::Context::DecidePluginStatus(
    const GetPluginInfo_Params& params,
    const WebPluginInfo& plugin,
    PluginFinder* plugin_finder,
    ChromeViewHostMsg_GetPluginInfo_Status* status) const {
  scoped_ptr<webkit::npapi::PluginGroup> group(
      webkit::npapi::PluginList::Singleton()->GetPluginGroup(plugin));

  ContentSetting plugin_setting = CONTENT_SETTING_DEFAULT;
  bool uses_default_content_setting = true;
  // Check plug-in content settings. The primary URL is the top origin URL and
  // the secondary URL is the plug-in URL.
  GetPluginContentSetting(plugin, params.top_origin_url, params.url,
                          group->identifier(), &plugin_setting,
                          &uses_default_content_setting);
  DCHECK(plugin_setting != CONTENT_SETTING_DEFAULT);

#if defined(ENABLE_PLUGIN_INSTALLATION)
#if defined(OS_LINUX)
  // On Linux, unknown plugins require authorization.
  PluginInstaller::SecurityStatus plugin_status =
      PluginInstaller::SECURITY_STATUS_REQUIRES_AUTHORIZATION;
#else
  PluginInstaller::SecurityStatus plugin_status =
      PluginInstaller::SECURITY_STATUS_UP_TO_DATE;
#endif
  PluginInstaller* installer =
      plugin_finder->FindPluginWithIdentifier(group->identifier());
  if (installer)
    plugin_status = installer->GetSecurityStatus(plugin);
  // Check if the plug-in is outdated.
  if (plugin_status == PluginInstaller::SECURITY_STATUS_OUT_OF_DATE &&
      !allow_outdated_plugins_.GetValue()) {
    if (allow_outdated_plugins_.IsManaged()) {
      status->value =
          ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedDisallowed;
    } else {
      status->value = ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked;
    }
    return;
  }

  // Check if the plug-in requires authorization.
  if ((plugin_status ==
           PluginInstaller::SECURITY_STATUS_REQUIRES_AUTHORIZATION ||
       PluginService::GetInstance()->IsPluginUnstable(plugin.path)) &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS &&
      !always_authorize_plugins_.GetValue() &&
      plugin_setting != CONTENT_SETTING_BLOCK &&
      uses_default_content_setting) {
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized;
    return;
  }
#endif

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
    std::string* actual_mime_type) const {
  bool allow_wildcard = true;
  std::vector<WebPluginInfo> matching_plugins;
  std::vector<std::string> mime_types;
  PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, allow_wildcard, &matching_plugins, &mime_types);
  content::PluginServiceFilter* filter =
      PluginService::GetInstance()->GetFilter();
  bool found = false;
  for (size_t i = 0; i < matching_plugins.size(); ++i) {
    bool enabled = !filter || filter->ShouldUsePlugin(render_process_id_,
                                                      render_view_id,
                                                      resource_context_,
                                                      url,
                                                      top_origin_url,
                                                      &matching_plugins[i]);
    if (!found || enabled) {
      *plugin = matching_plugins[i];
      *actual_mime_type = mime_types[i];
      if (enabled) {
        // We have found an enabled plug-in. Return immediately.
        return true;
      }
      // We have found a plug-in, but it's disabled. Keep looking for an
      // enabled one.
      found = true;
    }
  }

  // If we're here and have previously found a plug-in, it must have been
  // disabled.
  if (found)
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kDisabled;
  else
    status->value = ChromeViewHostMsg_GetPluginInfo_Status::kNotFound;
  return false;
}

void PluginInfoMessageFilter::Context::GetPluginContentSetting(
    const WebPluginInfo& plugin,
    const GURL& policy_url,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    bool* uses_default_content_setting) const {
  // Treat Native Client invocations like Javascript.
  bool is_nacl_plugin = (plugin.name == ASCIIToUTF16(
      chrome::ChromeContentClient::kNaClPluginName));

  scoped_ptr<base::Value> value;
  content_settings::SettingInfo info;
  bool uses_plugin_specific_setting = false;
  if (is_nacl_plugin) {
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
