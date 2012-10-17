// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_
#define CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_instance.h"

class FilePath;

namespace IPC {
struct ChannelHandle;
}

namespace ppapi {
class PpapiPermissions;
namespace host {
class PpapiHost;
}
}

namespace WebKit {
class WebPluginContainer;
}

namespace webkit {
namespace ppapi {
class PluginInstance;
class PluginModule;
}
}

namespace content {

class RenderView;

// Interface that allows components in the embedder app to talk to the
// PpapiHost in the renderer process.
//
// There will be one of these objects in the renderer per plugin module.
class RendererPpapiHost {
 public:
  // Creates a host and sets up an out-of-process proxy for an external plugin
  // module. |file_path| should identify the module. It is only used to report
  // failures to the renderer.
  // Returns a host if the external module is proxied successfully, otherwise
  // returns NULL.
  CONTENT_EXPORT static RendererPpapiHost* CreateExternalPluginModule(
      scoped_refptr<webkit::ppapi::PluginModule> plugin_module,
      webkit::ppapi::PluginInstance* plugin_instance,
      const FilePath& file_path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      int plugin_child_id);

  // Returns the PpapiHost object.
  virtual ppapi::host::PpapiHost* GetPpapiHost() = 0;

  // Returns true if the given PP_Instance is valid and belongs to the
  // plugin associated with this host.
  virtual bool IsValidInstance(PP_Instance instance) const = 0;

  // Returns the RenderView for the given plugin instance, or NULL if the
  // instance is invalid.
  virtual RenderView* GetRenderViewForInstance(PP_Instance instance) const = 0;

  // Returns the WebPluginContainer for the given plugin instance, or NULL if
  // the instance is invalid.
  virtual WebKit::WebPluginContainer* GetContainerForInstance(
      PP_Instance instance) const = 0;

  // Returns true if the given instance is considered to be currently
  // processing a user gesture or the plugin module has the "override user
  // gesture" flag set (in which case it can always do things normally
  // restricted by user gestures). Returns false if the instance is invalid or
  // if there is no current user gesture.
  virtual bool HasUserGesture(PP_Instance instance) const = 0;

 protected:
  virtual ~RendererPpapiHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_

