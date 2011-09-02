// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_FILTER_H_
#define CONTENT_BROWSER_PLUGIN_FILTER_H_
#pragma once

class GURL;

namespace webkit {
struct WebPluginInfo;
}

namespace content {

class ResourceContext;

// Callback class to let the client filter the list of all installed plug-ins.
// This class is called on the FILE thread.
class PluginServiceFilter {
 public:
  virtual ~PluginServiceFilter() {}
  // Whether to use |plugin|. The client can return false to disallow the
  // plugin, or return true and optionally change the passed in plugin.
  virtual bool ShouldUsePlugin(
      int render_process_id,
      int render_view_id,
      const void* context,
      const GURL& url,
      const GURL& policy_url,
      webkit::WebPluginInfo* plugin) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_FILTER_H_
