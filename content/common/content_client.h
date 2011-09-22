// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CLIENT_H_
#define CONTENT_COMMON_CONTENT_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

class CommandLine;
class GURL;
struct GPUInfo;
struct PepperPluginInfo;

namespace IPC {
class Message;
}

namespace base {
class StringPiece;
}

namespace sandbox {
class TargetPolicy;
}

namespace content {

class ContentBrowserClient;
class ContentClient;
class ContentPluginClient;
class ContentRendererClient;
class ContentUtilityClient;

// Setter and getter for the client.  The client should be set early, before any
// content code is called.
CONTENT_EXPORT void SetContentClient(ContentClient* client);
CONTENT_EXPORT ContentClient* GetContentClient();

// Interface that the embedder implements.
class CONTENT_EXPORT ContentClient {
 public:
  ContentClient();
  virtual ~ContentClient();

  ContentBrowserClient* browser() { return browser_; }
  void set_browser(ContentBrowserClient* c) { browser_ = c; }
  ContentPluginClient* plugin() { return plugin_; }
  void set_plugin(ContentPluginClient* p) { plugin_ = p; }
  ContentRendererClient* renderer() { return renderer_; }
  void set_renderer(ContentRendererClient* r) { renderer_ = r; }
  ContentUtilityClient* utility() { return utility_; }
  void set_utility(ContentUtilityClient* u) { utility_ = u; }

  // Sets the currently active URL.  Use GURL() to clear the URL.
  virtual void SetActiveURL(const GURL& url) = 0;

  // Sets the data on the current gpu.
  virtual void SetGpuInfo(const GPUInfo& gpu_info) = 0;

  // Gives the embedder a chance to register its own pepper plugins.
  virtual void AddPepperPlugins(std::vector<PepperPluginInfo>* plugins) = 0;

  // Returns whether the given message should be allowed to be sent from a
  // swapped out renderer.
  virtual bool CanSendWhileSwappedOut(const IPC::Message* msg) = 0;

  // Returns whether the given message should be processed in the browser on
  // behalf of a swapped out renderer.
  virtual bool CanHandleWhileSwappedOut(const IPC::Message& msg) = 0;

  // Returns the user agent. If mimic_windows is true then the embedder can
  // return a fake Windows user agent. This is a workaround for broken
  // websites.
  virtual std::string GetUserAgent(bool mimic_windows) const = 0;

  // Returns a string resource given its id.
  virtual string16 GetLocalizedString(int message_id) const = 0;

  // Return the contents of a resource in a StringPiece given the resource id.
  virtual base::StringPiece GetDataResource(int resource_id) const = 0;

#if defined(OS_WIN)
  // Allows the embedder to sandbox a plugin, and apply a custom policy.
  virtual bool SandboxPlugin(CommandLine* command_line,
                             sandbox::TargetPolicy* policy) = 0;
#endif

 private:
  // The embedder API for participating in browser logic.
  ContentBrowserClient* browser_;
  // The embedder API for participating in plugin logic.
  ContentPluginClient* plugin_;
  // The embedder API for participating in renderer logic.
  ContentRendererClient* renderer_;
  // The embedder API for participating in utility logic.
  ContentUtilityClient* utility_;
};

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_CLIENT_H_
