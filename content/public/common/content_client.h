// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

class CommandLine;
class GURL;

namespace IPC {
class Message;
}

namespace base {
class StringPiece;
}

namespace sandbox {
class TargetPolicy;
}

namespace webkit {
namespace npapi {
class PluginList;
}
}

namespace content {

class ContentBrowserClient;
class ContentClient;
class ContentPluginClient;
class ContentRendererClient;
class ContentUtilityClient;
struct GPUInfo;
struct PepperPluginInfo;

// Setter and getter for the client.  The client should be set early, before any
// content code is called.
CONTENT_EXPORT void SetContentClient(ContentClient* client);
CONTENT_EXPORT ContentClient* GetContentClient();

// Returns the user agent string being used by the browser. SetContentClient()
// must be called prior to calling this, and this routine must be used
// instead of webkit_glue::GetUserAgent() in order to ensure that we use
// the same user agent string everywhere.
// TODO(dpranke): This is caused by webkit_glue being a library that can
// get linked into multiple linkable objects, causing us to have multiple
// static values of the user agent. This will be fixed when we clean up
// webkit_glue.
CONTENT_EXPORT const std::string& GetUserAgent(const GURL& url);

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
  virtual void SetGpuInfo(const content::GPUInfo& gpu_info) = 0;

  // Gives the embedder a chance to register its own pepper plugins.
  virtual void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) = 0;

  // Gives the embedder a chance to register its own internal NPAPI plugins.
  virtual void AddNPAPIPlugins(
      webkit::npapi::PluginList* plugin_list) = 0;

  // Returns whether the given message should be allowed to be sent from a
  // swapped out renderer.
  virtual bool CanSendWhileSwappedOut(const IPC::Message* msg) = 0;

  // Returns whether the given message should be processed in the browser on
  // behalf of a swapped out renderer.
  virtual bool CanHandleWhileSwappedOut(const IPC::Message& msg) = 0;

  // Returns the user agent and a flag indicating whether the returned
  // string should always be used (if false, callers may override the
  // value as needed to work around various user agent sniffing bugs).
  virtual std::string GetUserAgent(bool *overriding) const = 0;

  // Returns a string resource given its id.
  virtual string16 GetLocalizedString(int message_id) const = 0;

  // Return the contents of a resource in a StringPiece given the resource id.
  virtual base::StringPiece GetDataResource(int resource_id) const = 0;

#if defined(OS_WIN)
  // Allows the embedder to sandbox a plugin, and apply a custom policy.
  virtual bool SandboxPlugin(CommandLine* command_line,
                             sandbox::TargetPolicy* policy) = 0;
#endif

#if defined(OS_MACOSX)
  // Allows the embedder to define a new |sandbox_type| by mapping it to the
  // resource ID corresponding to the sandbox profile to use. The legal values
  // for |sandbox_type| are defined by the embedder and should start with
  // SandboxType::SANDBOX_TYPE_AFTER_LAST_TYPE. Returns false if no sandbox
  // profile for the given |sandbox_type| exists. Otherwise,
  // |sandbox_profile_resource_id| is set to the resource ID corresponding to
  // the sandbox profile to use and true is returned.
  virtual bool GetSandboxProfileForSandboxType(
      int sandbox_type,
      int* sandbox_profile_resource_id) const = 0;
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

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_
