// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INFO_MESSAGE_FILTER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INFO_MESSAGE_FILTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/prefs/pref_member.h"
#include "components/ukm/ukm_service.h"
#include "content/public/browser/browser_message_filter.h"
#include "extensions/features/features.h"
#include "media/media_features.h"

struct ChromeViewHostMsg_GetPluginInfo_Output;
enum class ChromeViewHostMsg_GetPluginInfo_Status;
class GURL;
class HostContentSettingsMap;
class Profile;

namespace base {
class SingleThreadTaskRunner;
}

namespace component_updater {
struct ComponentInfo;
}

namespace content {
class ResourceContext;
struct WebPluginInfo;
}

namespace extensions {
class ExtensionRegistry;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace url {
class Origin;
}

// This class filters out incoming IPC messages requesting plugin information.
class PluginInfoMessageFilter : public content::BrowserMessageFilter {
 public:
  struct GetPluginInfo_Params;

  // Contains all the information needed by the PluginInfoMessageFilter.
  class Context {
   public:
    Context(int render_process_id, Profile* profile);

    ~Context();

    int render_process_id() { return render_process_id_; }

    void DecidePluginStatus(
        const GURL& url,
        const url::Origin& main_frame_origin,
        const content::WebPluginInfo& plugin,
        PluginMetadata::SecurityStatus security_status,
        const std::string& plugin_identifier,
        ChromeViewHostMsg_GetPluginInfo_Status* status) const;
    bool FindEnabledPlugin(
        int render_frame_id,
        const GURL& url,
        const url::Origin& main_frame_origin,
        const std::string& mime_type,
        ChromeViewHostMsg_GetPluginInfo_Status* status,
        content::WebPluginInfo* plugin,
        std::string* actual_mime_type,
        std::unique_ptr<PluginMetadata>* plugin_metadata) const;
    void MaybeGrantAccess(ChromeViewHostMsg_GetPluginInfo_Status status,
                          const base::FilePath& path) const;
    bool IsPluginEnabled(const content::WebPluginInfo& plugin) const;

    void ShutdownOnUIThread();

   private:
    int render_process_id_;
    content::ResourceContext* resource_context_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::ExtensionRegistry* extension_registry_;
#endif
    const HostContentSettingsMap* host_content_settings_map_;
    scoped_refptr<PluginPrefs> plugin_prefs_;

    BooleanPrefMember allow_outdated_plugins_;
    BooleanPrefMember always_authorize_plugins_;
    BooleanPrefMember run_all_flash_in_allow_mode_;
  };

  PluginInfoMessageFilter(int render_process_id, Profile* profile);

  // content::BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<PluginInfoMessageFilter>;

  void ShutdownOnUIThread();
  ~PluginInfoMessageFilter() override;

  void OnGetPluginInfo(int render_frame_id,
                       const GURL& url,
                       const url::Origin& main_frame_origin,
                       const std::string& mime_type,
                       IPC::Message* reply_msg);

  // |params| wraps the parameters passed to |OnGetPluginInfo|, because
  // |base::Bind| doesn't support the required arity <http://crbug.com/98542>.
  void PluginsLoaded(const GetPluginInfo_Params& params,
                     IPC::Message* reply_msg,
                     const std::vector<content::WebPluginInfo>& plugins);

  void ComponentPluginLookupDone(
      const GetPluginInfo_Params& params,
      std::unique_ptr<ChromeViewHostMsg_GetPluginInfo_Output> output,
      std::unique_ptr<PluginMetadata> plugin_metadata,
      IPC::Message* reply_msg,
      std::unique_ptr<component_updater::ComponentInfo> cus_plugin_info);

  void GetPluginInfoReply(
      const GetPluginInfo_Params& params,
      std::unique_ptr<ChromeViewHostMsg_GetPluginInfo_Output> output,
      std::unique_ptr<PluginMetadata> plugin_metadata,
      IPC::Message* reply_msg);

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Returns whether any internal plugin supporting |mime_type| is registered
  // and enabled. Does not determine whether the plugin can actually be
  // instantiated (e.g. whether it has all its dependencies).
  // When the returned *|is_available| is true, |additional_param_names| and
  // |additional_param_values| contain the name-value pairs, if any, specified
  // for the *first* non-disabled plugin found that is registered for
  // |mime_type|.
  void OnIsInternalPluginAvailableForMimeType(
      const std::string& mime_type,
      bool* is_available,
      std::vector<base::string16>* additional_param_names,
      std::vector<base::string16>* additional_param_values);
#endif

  // Reports usage metrics to RAPPOR and UKM. This must be a class function,
  // because UkmService requires a friend declaration by design to call.
  void ReportMetrics(int render_frame_id,
                     const base::StringPiece& mime_type,
                     const GURL& url,
                     const url::Origin& main_frame_origin,
                     ukm::SourceId ukm_source_id);

  Context context_;
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_notifier_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  const ukm::SourceId ukm_source_id_;

  DISALLOW_COPY_AND_ASSIGN(PluginInfoMessageFilter);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INFO_MESSAGE_FILTER_H_
