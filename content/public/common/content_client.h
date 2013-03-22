// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ui/base/layout.h"

class CommandLine;
class GURL;

namespace base {
class RefCountedStaticMemory;
}

namespace IPC {
class Message;
}

namespace gfx {
class Image;
}

namespace sandbox {
class TargetPolicy;
}

namespace webkit {
namespace npapi {
class PluginList;
}

namespace ppapi {
class HostGlobals;
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

// Returns the PPAPI global singleton. See webkit/plugins/ppapi/host_globals.h
// TODO(dpranke): Also needed since webkit_glue is a library.
CONTENT_EXPORT webkit::ppapi::HostGlobals* GetHostGlobals();

// Interface that the embedder implements.
class CONTENT_EXPORT ContentClient {
 public:
  ContentClient();
  virtual ~ContentClient();

  ContentBrowserClient* browser() { return browser_; }
  ContentPluginClient* plugin() { return plugin_; }
  ContentRendererClient* renderer() { return renderer_; }
  ContentUtilityClient* utility() { return utility_; }

  // Sets the currently active URL.  Use GURL() to clear the URL.
  virtual void SetActiveURL(const GURL& url) {}

  // Sets the data on the current gpu.
  virtual void SetGpuInfo(const content::GPUInfo& gpu_info) {}

  // Gives the embedder a chance to register its own pepper plugins.
  virtual void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) {}

  // Gives the embedder a chance to register its own internal NPAPI plugins.
  virtual void AddNPAPIPlugins(
      webkit::npapi::PluginList* plugin_list) {}

  // Gives the embedder a chance to register its own standard and saveable
  // url schemes early on in the startup sequence.
  virtual void AddAdditionalSchemes(
      std::vector<std::string>* standard_schemes,
      std::vector<std::string>* savable_schemes) {}

  // Returns whether the given message should be sent in a swapped out renderer.
  virtual bool CanSendWhileSwappedOut(const IPC::Message* message);

  // Returns whether the given message should be processed in the browser on
  // behalf of a swapped out renderer.
  virtual bool CanHandleWhileSwappedOut(const IPC::Message& message);

  // Returns a string describing the embedder version.  Used as part of the
  // user agent string.
  virtual std::string GetProduct() const;

  // Returns the user agent.
  virtual std::string GetUserAgent() const;

  // Returns a string resource given its id.
  virtual string16 GetLocalizedString(int message_id) const;

  // Return the contents of a resource in a StringPiece given the resource id.
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const;

  // Returns the raw bytes of a scale independent data resource.
  virtual base::RefCountedStaticMemory* GetDataResourceBytes(
      int resource_id) const;

  // Returns a native image given its id.
  virtual gfx::Image& GetNativeImageNamed(int resource_id) const;

  // Called by content::GetProcessTypeNameInEnglish for process types that it
  // doesn't know about because they're from the embedder.
  virtual std::string GetProcessTypeNameInEnglish(int type);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Allows the embedder to define a new |sandbox_type| by mapping it to the
  // resource ID corresponding to the sandbox profile to use. The legal values
  // for |sandbox_type| are defined by the embedder and should start with
  // SandboxType::SANDBOX_TYPE_AFTER_LAST_TYPE. Returns false if no sandbox
  // profile for the given |sandbox_type| exists. Otherwise,
  // |sandbox_profile_resource_id| is set to the resource ID corresponding to
  // the sandbox profile to use and true is returned.
  virtual bool GetSandboxProfileForSandboxType(
      int sandbox_type,
      int* sandbox_profile_resource_id) const;

  // Gets the Carbon interposing path to give to DYLD. Returns an empty string
  // if the embedder doesn't bundle it.
  virtual std::string GetCarbonInterposePath() const;
#endif

  void set_browser_for_testing(ContentBrowserClient* c) { browser_ = c; }
  void set_renderer_for_testing(ContentRendererClient* r) { renderer_ = r; }

 private:
  friend class ContentClientInitializer;  // To set these pointers.

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
