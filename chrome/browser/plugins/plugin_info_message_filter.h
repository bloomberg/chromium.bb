// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INFO_MESSAGE_FILTER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INFO_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/public/pref_member.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/browser_message_filter.h"

struct ChromeViewHostMsg_GetPluginInfo_Output;
struct ChromeViewHostMsg_GetPluginInfo_Status;
class GURL;
class HostContentSettingsMap;
class PluginFinder;
class PluginMetadata;
class Profile;

namespace content {
class ResourceContext;
}

namespace webkit {
struct WebPluginInfo;
}

// This class filters out incoming IPC messages requesting plug-in information.
class PluginInfoMessageFilter : public content::BrowserMessageFilter {
 public:
  struct GetPluginInfo_Params;

  // Contains all the information needed by the PluginInfoMessageFilter.
  class Context {
   public:
    Context(int render_process_id, Profile* profile);
    // Dummy constructor for tests.
    Context();
    ~Context();

    void DecidePluginStatus(
        const GetPluginInfo_Params& params,
        const webkit::WebPluginInfo& plugin,
        const PluginMetadata* plugin_metadata,
        ChromeViewHostMsg_GetPluginInfo_Status* status) const;
    bool FindEnabledPlugin(int render_view_id,
                           const GURL& url,
                           const GURL& top_origin_url,
                           const std::string& mime_type,
                           ChromeViewHostMsg_GetPluginInfo_Status* status,
                           webkit::WebPluginInfo* plugin,
                           std::string* actual_mime_type,
                           scoped_ptr<PluginMetadata>* plugin_metadata) const;
    void GetPluginContentSetting(const webkit::WebPluginInfo& plugin,
                                 const GURL& policy_url,
                                 const GURL& plugin_url,
                                 const std::string& resource,
                                 ContentSetting* setting,
                                 bool* is_default) const;

   private:
    int render_process_id_;
    content::ResourceContext* resource_context_;
    const HostContentSettingsMap* host_content_settings_map_;

    BooleanPrefMember allow_outdated_plugins_;
    BooleanPrefMember always_authorize_plugins_;
  };

  PluginInfoMessageFilter(int render_process_id, Profile* profile);

  // content::BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<PluginInfoMessageFilter>;

  virtual ~PluginInfoMessageFilter();

  void OnGetPluginInfo(int render_view_id,
                       const GURL& url,
                       const GURL& top_origin_url,
                       const std::string& mime_type,
                       IPC::Message* reply_msg);

  // |params| wraps the parameters passed to |OnGetPluginInfo|, because
  // |base::Bind| doesn't support the required arity <http://crbug.com/98542>.
  void PluginsLoaded(const GetPluginInfo_Params& params,
                     IPC::Message* reply_msg,
                     const std::vector<webkit::WebPluginInfo>& plugins);

  Context context_;

  base::WeakPtrFactory<PluginInfoMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginInfoMessageFilter);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INFO_MESSAGE_FILTER_H_
