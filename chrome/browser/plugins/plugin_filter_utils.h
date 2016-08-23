// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_FILTER_UTILS_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_FILTER_UTILS_H_

#include "components/content_settings/core/common/content_settings.h"

class GURL;
class HostContentSettingsMap;

namespace content {
struct WebPluginInfo;
}

// |is_default| and |is_managed| may be nullptr. In that case, they aren't set.
void GetPluginContentSetting(
    const HostContentSettingsMap* host_content_settings_map,
    const content::WebPluginInfo& plugin,
    const GURL& policy_url,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    bool* is_default,
    bool* is_managed);

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_FILTER_UTILS_H_
