// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_LOADER_POSIX_H_
#define CONTENT_BROWSER_PLUGIN_LOADER_POSIX_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "content/browser/plugin_service.h"
#include "content/browser/utility_process_host.h"
#include "webkit/plugins/webplugininfo.h"

namespace base {
class MessageLoopProxy;
}

class PluginLoaderPosix : public UtilityProcessHost::Client {
 public:
  // Must be called on the IO thread.
  static void LoadPlugins(base::MessageLoopProxy* target_loop,
                          const PluginService::GetPluginsCallback& callback);

  // UtilityProcessHost::Client:
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  PluginLoaderPosix(base::MessageLoopProxy* target_loop,
                    const PluginService::GetPluginsCallback& callback);
  virtual ~PluginLoaderPosix();

  void OnGotPlugins(const std::vector<webkit::WebPluginInfo>& plugins);

  // The callback and message loop on which the callback will be run when the
  // plugin loading process has been completed.
  scoped_refptr<base::MessageLoopProxy> target_loop_;
  PluginService::GetPluginsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PluginLoaderPosix);
};

#endif  // CONTENT_BROWSER_PLUGIN_LOADER_POSIX_H_
