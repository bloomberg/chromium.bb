// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/shared_impl/dir_contents.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"
#include "webkit/common/fileapi/file_system_types.h"

class GURL;
class SkCanvas;
class TransportDIB;
struct PP_NetAddress_Private;

namespace IPC {
struct ChannelHandle;
}

namespace WebKit {
class WebGraphicsContext3D;
}

namespace base {
class MessageLoopProxy;
class Time;
}

namespace content {
class RendererPpapiHost;
}

namespace fileapi {
struct DirectoryEntry;
}

namespace gfx {
class Point;
}

namespace ppapi {
class PepperFilePath;
class PpapiPermissions;
class SocketOptionData;
struct DeviceRefData;
struct HostPortPair;

}  // namespace ppapi

namespace WebKit {
typedef SkCanvas WebCanvas;
class WebGamepads;
struct WebCursorInfo;
struct WebURLError;
class WebURLLoaderClient;
class WebURLResponse;
}

namespace content {

class FileIO;
class FullscreenContainer;
class PepperPluginInstanceImpl;
class PluginModule;
class PPB_Flash_Menu_Impl;
class PPB_ImageData_Impl;
class PPB_TCPSocket_Private_Impl;

// Virtual interface that the browser implements to implement features for
// PPAPI plugins.
class PluginDelegate {
 public:
  // Notification that the given plugin is focused or unfocused.
  virtual void PluginFocusChanged(PepperPluginInstanceImpl* instance,
                                  bool focused) = 0;
  // Notification that the text input status of the given plugin is changed.
  virtual void PluginTextInputTypeChanged(
      PepperPluginInstanceImpl* instance) = 0;
  // Notification that the caret position in the given plugin is changed.
  virtual void PluginCaretPositionChanged(
      PepperPluginInstanceImpl* instance) = 0;
  // Notification that the plugin requested to cancel the current composition.
  virtual void PluginRequestedCancelComposition(
      PepperPluginInstanceImpl* instance) = 0;
  // Notification that the text selection in the given plugin is changed.
  virtual void PluginSelectionChanged(PepperPluginInstanceImpl* instance) = 0;

  // Indicates that the given instance has been created.
  virtual void InstanceCreated(PepperPluginInstanceImpl* instance) = 0;

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  virtual void InstanceDeleted(PepperPluginInstanceImpl* instance) = 0;

  // Sends an async IPC to open a local file.
  typedef base::Callback<void (base::PlatformFileError, base::PassPlatformFile)>
      AsyncOpenFileCallback;
  virtual bool AsyncOpenFile(const base::FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback) = 0;

  // Returns a MessageLoopProxy instance associated with the message loop
  // of the file thread in this renderer.
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() = 0;

  // Retrieve current gamepad data.
  virtual void SampleGamepads(WebKit::WebGamepads* data) = 0;

  // Notifies the plugin of the document load. This should initiate the call to
  // PPP_Instance.HandleDocumentLoad.
  //
  // The loader object should set itself on the PluginInstance as the document
  // loader using set_document_loader.
  virtual void HandleDocumentLoad(PepperPluginInstanceImpl* instance,
                                  const WebKit::WebURLResponse& response) = 0;

  // Sets up the renderer host and out-of-process proxy for an external plugin
  // module. Returns the renderer host, or NULL if it couldn't be created.
  virtual RendererPpapiHost* CreateExternalPluginModule(
      scoped_refptr<PluginModule> module,
      const base::FilePath& path,
      ::ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_DELEGATE_H_
