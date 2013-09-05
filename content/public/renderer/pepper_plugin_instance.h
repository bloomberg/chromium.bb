// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_PEPPER_PLUGIN_INSTANCE_H_
#define CONTENT_PUBLIC_RENDERER_PEPPER_PLUGIN_INSTANCE_H_

#include "base/basictypes.h"
#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_instance_private.h"

class GURL;

namespace base {
class FilePath;
}

namespace gfx {
class ImageSkia;
class Rect;
}

namespace ppapi {
class PpapiPermissions;
class VarTracker;
struct URLRequestInfoData;
}

namespace IPC {
struct ChannelHandle;
}

namespace WebKit {
class WebPluginContainer;
}

namespace content {
class RenderView;

class PepperPluginInstance {
 public:
  static CONTENT_EXPORT PepperPluginInstance* Get(PP_Instance instance_id);

  virtual ~PepperPluginInstance() {}

  virtual content::RenderView* GetRenderView() = 0;

  virtual WebKit::WebPluginContainer* GetContainer() = 0;

  virtual ppapi::VarTracker* GetVarTracker() = 0;

  virtual const GURL& GetPluginURL() = 0;

  // Returns the location of this module.
  virtual base::FilePath GetModulePath() = 0;

  // Returns a reference to a file with the given path.
  // The returned object will have a refcount of 0 (just like "new").
  virtual PP_Resource CreateExternalFileReference(
      const base::FilePath& external_file_path) = 0;

  // Creates a PPB_ImageData given a Skia image.
  virtual PP_Resource CreateImage(gfx::ImageSkia* source_image,
                                  float scale) = 0;

  // Switches this instance with one that uses the out of process IPC proxy.
  virtual PP_ExternalPluginResult SwitchToOutOfProcessProxy(
      const base::FilePath& file_path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) = 0;

  // Set this to true if plugin thinks it will always be on top. This allows us
  // to use a more optimized painting path in some cases.
  virtual void SetAlwaysOnTop(bool on_top) = 0;

  // Returns true iff the plugin is a full-page plugin (i.e. not in an iframe
  // or embedded in a page).
  virtual bool IsFullPagePlugin() = 0;

  // Switches between fullscreen and normal mode. If |delay_report| is set to
  // false, it may report the new state through DidChangeView immediately. If
  // true, it will delay it. When called from the plugin, delay_report should
  // be true to avoid re-entrancy.
  virtual void FlashSetFullscreen(bool fullscreen, bool delay_report) = 0;

  virtual bool IsRectTopmost(const gfx::Rect& rect) = 0;

  virtual int32_t Navigate(const ppapi::URLRequestInfoData& request,
                           const char* target,
                           bool from_user_action) = 0;

};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_PEPPER_PLUGIN_INSTANCE_H_
