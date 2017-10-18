// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_info_message_filter.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/plugins/plugins_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/nacl/common/features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_constants.h"
#include "extensions/features/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ppapi/features/features.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#endif

using content::PluginService;
using content::WebPluginInfo;

namespace {

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
          "PluginInfoMessageFilter") {}

  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

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

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Returns whether a request from a plugin to load |resource| from a renderer
// with process id |process_id| is a request for an internal resource by an app
// listed in |accessible_resources| in its manifest.
bool IsPluginLoadingAccessibleResourceInWebView(
    extensions::ExtensionRegistry* extension_registry,
    int process_id,
    const GURL& resource) {
  extensions::WebViewRendererState* renderer_state =
      extensions::WebViewRendererState::GetInstance();
  std::string partition_id;
  if (!renderer_state->IsGuest(process_id) ||
      !renderer_state->GetPartitionID(process_id, &partition_id)) {
    return false;
  }

  const std::string extension_id = resource.host();
  const extensions::Extension* extension = extension_registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension || !extensions::WebviewInfo::IsResourceWebviewAccessible(
          extension, partition_id, resource.path())) {
    return false;
  }

  // Make sure the renderer making the request actually belongs to the
  // same extension.
  std::string owner_extension;
  return renderer_state->GetOwnerInfo(process_id, nullptr, &owner_extension) &&
         owner_extension == extension_id;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

PluginInfoMessageFilter::Context::Context(int render_process_id,
                                          Profile* profile)
    : render_process_id_(render_process_id),
      resource_context_(profile->GetResourceContext()),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_registry_(extensions::ExtensionRegistry::Get(profile)),
#endif
      host_content_settings_map_(HostContentSettingsMapFactory::GetForProfile(
          profile)),
      plugin_prefs_(PluginPrefs::GetForProfile(profile)) {
  allow_outdated_plugins_.Init(prefs::kPluginsAllowOutdated,
                               profile->GetPrefs());
  allow_outdated_plugins_.MoveToThread(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO));
  always_authorize_plugins_.Init(prefs::kPluginsAlwaysAuthorize,
                                 profile->GetPrefs());
  always_authorize_plugins_.MoveToThread(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO));
  run_all_flash_in_allow_mode_.Init(prefs::kRunAllFlashInAllowMode,
                                    profile->GetPrefs());
  run_all_flash_in_allow_mode_.MoveToThread(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO));
}

PluginInfoMessageFilter::Context::~Context() {
}

void PluginInfoMessageFilter::Context::ShutdownOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  always_authorize_plugins_.Destroy();
  allow_outdated_plugins_.Destroy();
  run_all_flash_in_allow_mode_.Destroy();
}

PluginInfoMessageFilter::PluginInfoMessageFilter(int render_process_id,
                                                 Profile* profile)
    : BrowserMessageFilter(ChromeMsgStart),
      context_(render_process_id, profile),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      ukm_source_id_(ukm::UkmRecorder::GetNewSourceID()) {
  shutdown_notifier_ =
      ShutdownNotifierFactory::GetInstance()->Get(profile)->Subscribe(
          base::Bind(&PluginInfoMessageFilter::ShutdownOnUIThread,
                     base::Unretained(this)));
}

void PluginInfoMessageFilter::ShutdownOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  context_.ShutdownOnUIThread();
  shutdown_notifier_.reset();
}

bool PluginInfoMessageFilter::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PluginInfoMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChromeViewHostMsg_GetPluginInfo,
                                    OnGetPluginInfo)
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    IPC_MESSAGE_HANDLER(
        ChromeViewHostMsg_IsInternalPluginAvailableForMimeType,
        OnIsInternalPluginAvailableForMimeType)
#endif
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()
  return true;
}

void PluginInfoMessageFilter::OnDestruct() const {
  // Destroy on the UI thread because we contain a |PrefMember|.
  content::BrowserThread::DeleteOnUIThread::Destruct(this);
}

// static
void PluginInfoMessageFilter::RegisterUserPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kPluginsAllowOutdated, false);
  registry->RegisterBooleanPref(prefs::kPluginsAlwaysAuthorize, false);
  registry->RegisterBooleanPref(prefs::kRunAllFlashInAllowMode, false);
}

PluginInfoMessageFilter::~PluginInfoMessageFilter() {}

struct PluginInfoMessageFilter::GetPluginInfo_Params {
  int render_frame_id;
  GURL url;
  url::Origin main_frame_origin;
  std::string mime_type;
};

void PluginInfoMessageFilter::OnGetPluginInfo(
    int render_frame_id,
    const GURL& url,
    const url::Origin& main_frame_origin,
    const std::string& mime_type,
    IPC::Message* reply_msg) {
  GetPluginInfo_Params params = {render_frame_id, url, main_frame_origin,
                                 mime_type};
  PluginService::GetInstance()->GetPlugins(base::BindOnce(
      &PluginInfoMessageFilter::PluginsLoaded, this, params, reply_msg));
}

void PluginInfoMessageFilter::PluginsLoaded(
    const GetPluginInfo_Params& params,
    IPC::Message* reply_msg,
    const std::vector<WebPluginInfo>& plugins) {
  std::unique_ptr<ChromeViewHostMsg_GetPluginInfo_Output> output(
      new ChromeViewHostMsg_GetPluginInfo_Output());
  // This also fills in |actual_mime_type|.
  std::unique_ptr<PluginMetadata> plugin_metadata;
  if (context_.FindEnabledPlugin(params.render_frame_id, params.url,
                                 params.main_frame_origin, params.mime_type,
                                 &output->status, &output->plugin,
                                 &output->actual_mime_type, &plugin_metadata)) {
    context_.DecidePluginStatus(
        params.url, params.main_frame_origin, output->plugin,
        plugin_metadata->GetSecurityStatus(output->plugin),
        plugin_metadata->identifier(), &output->status);
  }

  if (output->status == ChromeViewHostMsg_GetPluginInfo_Status::kNotFound) {
    // Check to see if the component updater can fetch an implementation.
    base::PostTaskAndReplyWithResult(
        main_thread_task_runner_.get(), FROM_HERE,
        base::Bind(
            &component_updater::ComponentUpdateService::GetComponentForMimeType,
            base::Unretained(g_browser_process->component_updater()),
            params.mime_type),
        base::Bind(&PluginInfoMessageFilter::ComponentPluginLookupDone, this,
                   params, base::Passed(&output),
                   base::Passed(&plugin_metadata), reply_msg));
  } else {
    GetPluginInfoReply(params, std::move(output), std::move(plugin_metadata),
                       reply_msg);
  }
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

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

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

void PluginInfoMessageFilter::Context::DecidePluginStatus(
    const GURL& url,
    const url::Origin& main_frame_origin,
    const WebPluginInfo& plugin,
    PluginMetadata::SecurityStatus security_status,
    const std::string& plugin_identifier,
    ChromeViewHostMsg_GetPluginInfo_Status* status) const {
  if (security_status == PluginMetadata::SECURITY_STATUS_FULLY_TRUSTED) {
    *status = ChromeViewHostMsg_GetPluginInfo_Status::kAllowed;
    return;
  }

  ContentSetting plugin_setting = CONTENT_SETTING_DEFAULT;
  bool uses_default_content_setting = true;
  bool is_managed = false;
  // Check plugin content settings. The primary URL is the top origin URL and
  // the secondary URL is the plugin URL.
  PluginUtils::GetPluginContentSetting(
      host_content_settings_map_, plugin, main_frame_origin, url,
      plugin_identifier, &plugin_setting, &uses_default_content_setting,
      &is_managed);

  // TODO(tommycli): Remove once we deprecate the plugin ASK policy.
  bool legacy_ask_user = plugin_setting == CONTENT_SETTING_ASK;
  plugin_setting = PluginsFieldTrial::EffectiveContentSetting(
      host_content_settings_map_, CONTENT_SETTINGS_TYPE_PLUGINS,
      plugin_setting);

  DCHECK(plugin_setting != CONTENT_SETTING_DEFAULT);
  DCHECK(plugin_setting != CONTENT_SETTING_ASK);

  if (*status ==
      ChromeViewHostMsg_GetPluginInfo_Status::kFlashHiddenPreferHtml) {
    if (plugin_setting == CONTENT_SETTING_BLOCK) {
      *status = is_managed && !legacy_ask_user
                    ? ChromeViewHostMsg_GetPluginInfo_Status::kBlockedByPolicy
                    : ChromeViewHostMsg_GetPluginInfo_Status::kBlockedNoLoading;
    }
    return;
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  // Check if the plugin is outdated.
  if (security_status == PluginMetadata::SECURITY_STATUS_OUT_OF_DATE &&
      !allow_outdated_plugins_.GetValue()) {
    if (allow_outdated_plugins_.IsManaged()) {
      *status = ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedDisallowed;
    } else {
      *status = ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked;
    }
    return;
  }
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  // Check if the plugin is crashing too much.
  if (PluginService::GetInstance()->IsPluginUnstable(plugin.path) &&
      !always_authorize_plugins_.GetValue() &&
      plugin_setting != CONTENT_SETTING_BLOCK &&
      uses_default_content_setting) {
    *status = ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized;
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If an app has explicitly made internal resources available by listing them
  // in |accessible_resources| in the manifest, then allow them to be loaded by
  // plugins inside a guest-view.
  if (url.SchemeIs(extensions::kExtensionScheme) && !is_managed &&
      plugin_setting == CONTENT_SETTING_BLOCK &&
      IsPluginLoadingAccessibleResourceInWebView(extension_registry_,
                                                 render_process_id_, url)) {
    plugin_setting = CONTENT_SETTING_ALLOW;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  if (plugin_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT ||
      (plugin_setting == CONTENT_SETTING_ALLOW &&
       PluginUtils::ShouldPreferHtmlOverPlugins(host_content_settings_map_) &&
       !run_all_flash_in_allow_mode_.GetValue())) {
    *status = ChromeViewHostMsg_GetPluginInfo_Status::kPlayImportantContent;
  } else if (plugin_setting == CONTENT_SETTING_BLOCK) {
    // For managed users with the ASK policy, we allow manually running plugins
    // via context menu. This is the closest to admin intent.
    *status = is_managed && !legacy_ask_user
                  ? ChromeViewHostMsg_GetPluginInfo_Status::kBlockedByPolicy
                  : ChromeViewHostMsg_GetPluginInfo_Status::kBlocked;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Allow an embedder of <webview> to block a plugin from being loaded inside
  // the guest. In order to do this, set the status to 'Unauthorized' here,
  // and update the status as appropriate depending on the response from the
  // embedder.
  if (*status == ChromeViewHostMsg_GetPluginInfo_Status::kAllowed ||
      *status == ChromeViewHostMsg_GetPluginInfo_Status::kBlocked ||
      *status ==
          ChromeViewHostMsg_GetPluginInfo_Status::kPlayImportantContent) {
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(
            render_process_id_))
      *status = ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized;
  }
#endif
}

bool PluginInfoMessageFilter::Context::FindEnabledPlugin(
    int render_frame_id,
    const GURL& url,
    const url::Origin& main_frame_origin,
    const std::string& mime_type,
    ChromeViewHostMsg_GetPluginInfo_Status* status,
    WebPluginInfo* plugin,
    std::string* actual_mime_type,
    std::unique_ptr<PluginMetadata>* plugin_metadata) const {
  *status = ChromeViewHostMsg_GetPluginInfo_Status::kAllowed;

  bool allow_wildcard = true;
  std::vector<WebPluginInfo> matching_plugins;
  std::vector<std::string> mime_types;
  PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, allow_wildcard, &matching_plugins, &mime_types);
#if defined(GOOGLE_CHROME_BUILD)
  base::FilePath not_present =
      base::FilePath::FromUTF8Unsafe(ChromeContentClient::kNotPresent);
  matching_plugins.erase(
      std::remove_if(
          matching_plugins.begin(), matching_plugins.end(),
          [&not_present](const WebPluginInfo& info) {
            return info.path == not_present;
          }),
      matching_plugins.end());
#endif  // defined(GOOGLE_CHROME_BUILD)
  if (matching_plugins.empty()) {
    *status = ChromeViewHostMsg_GetPluginInfo_Status::kNotFound;
    return false;
  }

  content::PluginServiceFilter* filter =
      PluginService::GetInstance()->GetFilter();
  size_t i = 0;
  for (; i < matching_plugins.size(); ++i) {
    if (!filter ||
        filter->IsPluginAvailable(render_process_id_, render_frame_id,
                                  resource_context_, url, main_frame_origin,
                                  &matching_plugins[i])) {
      break;
    }
  }

  // If we broke out of the loop, we have found an enabled plugin.
  bool enabled = i < matching_plugins.size();
  if (!enabled) {
    // Otherwise, we only found disabled plugins, so we take the first one.
    i = 0;
    *status = ChromeViewHostMsg_GetPluginInfo_Status::kDisabled;

    if (PluginUtils::ShouldPreferHtmlOverPlugins(host_content_settings_map_) &&
        matching_plugins[0].name ==
            base::ASCIIToUTF16(content::kFlashPluginName)) {
      *status = ChromeViewHostMsg_GetPluginInfo_Status::kFlashHiddenPreferHtml;

      // In the Prefer HTML case, the plugin is actually enabled, but hidden.
      // It will still be blocked in the body of DecidePluginStatus.
      enabled = true;
    }
  }

  *plugin = matching_plugins[i];
  *actual_mime_type = mime_types[i];
  if (plugin_metadata)
    *plugin_metadata = PluginFinder::GetInstance()->GetPluginMetadata(*plugin);

  return enabled;
}

void PluginInfoMessageFilter::ComponentPluginLookupDone(
    const GetPluginInfo_Params& params,
    std::unique_ptr<ChromeViewHostMsg_GetPluginInfo_Output> output,
    std::unique_ptr<PluginMetadata> plugin_metadata,
    IPC::Message* reply_msg,
    std::unique_ptr<component_updater::ComponentInfo> cus_plugin_info) {
  if (cus_plugin_info) {
    output->status =
        ChromeViewHostMsg_GetPluginInfo_Status::kComponentUpdateRequired;
#if defined(OS_LINUX)
    if (cus_plugin_info->version != base::Version("0")) {
      output->status =
          ChromeViewHostMsg_GetPluginInfo_Status::kRestartRequired;
    }
#endif
    plugin_metadata = base::MakeUnique<PluginMetadata>(
        cus_plugin_info->id, cus_plugin_info->name, false, GURL(), GURL(),
        base::ASCIIToUTF16(cus_plugin_info->id), std::string());
  }
  GetPluginInfoReply(params, std::move(output), std::move(plugin_metadata),
                     reply_msg);
}

void PluginInfoMessageFilter::GetPluginInfoReply(
    const GetPluginInfo_Params& params,
    std::unique_ptr<ChromeViewHostMsg_GetPluginInfo_Output> output,
    std::unique_ptr<PluginMetadata> plugin_metadata,
    IPC::Message* reply_msg) {
  if (plugin_metadata) {
    output->group_identifier = plugin_metadata->identifier();
    output->group_name = plugin_metadata->name();
  }

  context_.MaybeGrantAccess(output->status, output->plugin.path);

  ChromeViewHostMsg_GetPluginInfo::WriteReplyParams(reply_msg, *output);
  Send(reply_msg);
  if (output->status != ChromeViewHostMsg_GetPluginInfo_Status::kNotFound) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PluginInfoMessageFilter::ReportMetrics, this,
                       params.render_frame_id, output->actual_mime_type,
                       params.url, params.main_frame_origin, ukm_source_id_));
  }
}

void PluginInfoMessageFilter::ReportMetrics(
    int render_frame_id,
    const base::StringPiece& mime_type,
    const GURL& url,
    const url::Origin& main_frame_origin,
    ukm::SourceId ukm_source_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      context_.render_process_id(), render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  // This can occur the web contents has already been closed or navigated away.
  if (!web_contents)
    return;

  if (web_contents->GetBrowserContext()->IsOffTheRecord())
    return;

  rappor::RapporServiceImpl* rappor_service =
      g_browser_process->rappor_service();
  if (!rappor_service)
    return;
  if (main_frame_origin.unique())
    return;

  if (mime_type != content::kFlashPluginSwfMimeType &&
      mime_type != content::kFlashPluginSplMimeType) {
    return;
  }

  rappor_service->RecordSampleString(
      "Plugins.FlashOriginUrl", rappor::ETLD_PLUS_ONE_RAPPOR_TYPE,
      net::registry_controlled_domains::GetDomainAndRegistry(
          main_frame_origin.GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  rappor_service->RecordSampleString(
      "Plugins.FlashUrl", rappor::ETLD_PLUS_ONE_RAPPOR_TYPE,
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;
  ukm_recorder->UpdateSourceURL(ukm_source_id,
                                web_contents->GetLastCommittedURL());
  // UkmEntryBuilder records the entry when it goes out of scope.
  std::unique_ptr<ukm::UkmEntryBuilder> builder =
      ukm_recorder->GetEntryBuilder(ukm_source_id, "Plugins.FlashInstance");
}

void PluginInfoMessageFilter::Context::MaybeGrantAccess(
    ChromeViewHostMsg_GetPluginInfo_Status status,
    const base::FilePath& path) const {
  if (status == ChromeViewHostMsg_GetPluginInfo_Status::kAllowed ||
      status == ChromeViewHostMsg_GetPluginInfo_Status::kPlayImportantContent) {
    ChromePluginServiceFilter::GetInstance()->AuthorizePlugin(
        render_process_id_, path);
  }
}

bool PluginInfoMessageFilter::Context::IsPluginEnabled(
    const content::WebPluginInfo& plugin) const {
  return plugin_prefs_->IsPluginEnabled(plugin);
}
