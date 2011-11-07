// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PLUGIN_INFO_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PLUGIN_INFO_MESSAGE_FILTER_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/common/content_settings.h"
#include "content/browser/browser_message_filter.h"

struct ChromeViewHostMsg_GetPluginInfo_Status;
class GURL;
class HostContentSettingsMap;
class Profile;

namespace content {
class ResourceContext;
}

namespace webkit {
struct WebPluginInfo;
}

// This class filters out incoming IPC messages requesting plug-in information.
class PluginInfoMessageFilter : public BrowserMessageFilter {
 public:
  PluginInfoMessageFilter(int render_process_id, Profile* profile);
  virtual ~PluginInfoMessageFilter();

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;

 private:
  struct GetPluginInfo_Params;

  void OnGetPluginInfo(int render_view_id,
                       const GURL& url,
                       const GURL& top_origin_url,
                       const std::string& mime_type,
                       IPC::Message* reply_msg);

  // Helper functions for |OnGetPluginInfo()|.

  // |params| wraps the parameters passed to |OnGetPluginInfo|, because
  // |base::Bind| doesn't support the required arity <http://crbug.com/98542>.
  void PluginsLoaded(const GetPluginInfo_Params& params,
                     IPC::Message* reply_msg,
                     const std::vector<webkit::WebPluginInfo>& plugins);
  void DecidePluginStatus(const GetPluginInfo_Params& params,
                          ChromeViewHostMsg_GetPluginInfo_Status* status,
                          webkit::WebPluginInfo* plugin,
                          std::string* actual_mime_type) const;
  bool FindEnabledPlugin(int render_view_id,
                         const GURL& url,
                         const GURL& top_origin_url,
                         const std::string& mime_type,
                         ChromeViewHostMsg_GetPluginInfo_Status* status,
                         webkit::WebPluginInfo* plugin,
                         std::string* actual_mime_type) const;
  void GetPluginContentSetting(const webkit::WebPluginInfo* plugin,
                               const GURL& policy_url,
                               const GURL& plugin_url,
                               const std::string& resource,
                               ContentSetting* setting,
                               bool* non_default) const;

  int render_process_id_;
  const content::ResourceContext& resource_context_;
  const HostContentSettingsMap* host_content_settings_map_;

  BooleanPrefMember allow_outdated_plugins_;
  BooleanPrefMember always_authorize_plugins_;

  base::WeakPtrFactory<PluginInfoMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginInfoMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
