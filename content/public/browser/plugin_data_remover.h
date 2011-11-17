// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLUGIN_DATA_REMOVER_H_
#define CONTENT_PUBLIC_BROWSER_PLUGIN_DATA_REMOVER_H_
#pragma once

#include "base/time.h"
#include "content/common/content_export.h"

namespace base {
class WaitableEvent;
}

namespace content {
class ResourceContext;
}

namespace webkit {
struct WebPluginInfo;
}

namespace content {

class CONTENT_EXPORT PluginDataRemover {
 public:
  static PluginDataRemover* Create(
      const content::ResourceContext& resource_context);
  virtual ~PluginDataRemover() {}

  // Starts removing plug-in data stored since |begin_time|.
  virtual base::WaitableEvent* StartRemoving(base::Time begin_time) = 0;

  // Returns whether there is a plug-in installed that supports removing LSO
  // data. If it returns true |plugin| is also set to that plugin. This method
  // will use cached plugin data. Call PluginService::GetPlugins() if the latest
  // data is needed.
  static bool IsSupported(webkit::WebPluginInfo* plugin);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLUGIN_DATA_REMOVER_H_
