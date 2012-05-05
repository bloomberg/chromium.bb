// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLUGIN_DATA_REMOVER_H_
#define CONTENT_PUBLIC_BROWSER_PLUGIN_DATA_REMOVER_H_
#pragma once

#include <vector>

#include "base/time.h"
#include "content/common/content_export.h"

namespace base {
class WaitableEvent;
}

namespace webkit {
struct WebPluginInfo;
}

namespace content {

class BrowserContext;

class CONTENT_EXPORT PluginDataRemover {
 public:
  static PluginDataRemover* Create(content::BrowserContext* browser_context);
  virtual ~PluginDataRemover() {}

  // Starts removing plug-in data stored since |begin_time|.
  virtual base::WaitableEvent* StartRemoving(base::Time begin_time) = 0;

  // Returns a list of all plug-ins that support removing LSO data. This method
  // will use cached plugin data. Call PluginService::GetPlugins() if the latest
  // data is needed.
  static void GetSupportedPlugins(std::vector<webkit::WebPluginInfo>* plugins);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLUGIN_DATA_REMOVER_H_
