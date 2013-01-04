// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_
#define CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_

#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "content/common/content_export.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

class FilePath;

namespace gfx {
class Point;
}

namespace IPC {
struct ChannelHandle;
}

namespace ppapi {
class PpapiPermissions;
namespace host {
class PpapiHost;
}
}

namespace webkit {
namespace ppapi {
class PluginInstance;
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
      base::ProcessId plugin_pid,
      int plugin_child_id);

  // Returns the RendererPpapiHost associated with the given PP_Instance,
  // or NULL if the instance is invalid.
  CONTENT_EXPORT static RendererPpapiHost* GetForPPInstance(
      PP_Instance instance);

  // Returns the PpapiHost object.
  virtual ppapi::host::PpapiHost* GetPpapiHost() = 0;

  // Returns true if the given PP_Instance is valid and belongs to the
  // plugin associated with this host.
  virtual bool IsValidInstance(PP_Instance instance) const = 0;

  // Returns the PluginInstance for the given PP_Instance, or NULL if the
  // PP_Instance is invalid (the common case this will be invalid is during
  // plugin teardown when resource hosts are being force-freed).
  virtual webkit::ppapi::PluginInstance* GetPluginInstance(
      PP_Instance instance) const = 0;

  // Returns the RenderView for the given plugin instance, or NULL if the
  // instance is invalid.
  virtual RenderView* GetRenderViewForInstance(PP_Instance instance) const = 0;

  // Returns the WebPluginContainer for the given plugin instance, or NULL if
  // the instance is invalid.
  virtual WebKit::WebPluginContainer* GetContainerForInstance(
      PP_Instance instance) const = 0;

  // Returns the PlatformGraphics2D for the given plugin resource, or NULL if
  // the resource is invalid.
  virtual webkit::ppapi::PluginDelegate::PlatformGraphics2D*
      GetPlatformGraphics2D(PP_Resource resource) = 0;

  // Returns true if the given instance is considered to be currently
  // processing a user gesture or the plugin module has the "override user
  // gesture" flag set (in which case it can always do things normally
  // restricted by user gestures). Returns false if the instance is invalid or
  // if there is no current user gesture.
  virtual bool HasUserGesture(PP_Instance instance) const = 0;

  // Returns the routing ID for the render widget containing the given
  // instance. This will take into account the current Flash fullscreen state,
  // so if there is a Flash fullscreen instance active, this will return the
  // routing ID of the fullscreen widget. Returns 0 on failure.
  virtual int GetRoutingIDForWidget(PP_Instance instance) const = 0;

  // Converts the given plugin coordinate to the containing RenderView. This
  // will take into account the current Flash fullscreen state so will use
  // the fullscreen widget if it's displayed.
  virtual gfx::Point PluginPointToRenderView(
      PP_Instance instance,
      const gfx::Point& pt) const = 0;

  // Shares a file handle (HANDLE / file descriptor) with the remote side. It
  // returns a handle that should be sent in exactly one IPC message. Upon
  // receipt, the remote side then owns that handle. Note: if sending the
  // message fails, the returned handle is properly closed by the IPC system. If
  // |should_close_source| is set to true, the original handle is closed by this
  // operation and should not be used again.
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      bool should_close_source) = 0;

  // Returns true if the plugin is running in process.
  virtual bool IsRunningInProcess() const = 0;

 protected:
  virtual ~RendererPpapiHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_
