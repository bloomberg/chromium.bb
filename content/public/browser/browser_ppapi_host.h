// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PPAPI_HOST_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PPAPI_HOST_H_

#include "base/callback_forward.h"
#include "base/process.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_view_host.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/pp_instance.h"

namespace IPC {
class ChannelProxy;
struct ChannelHandle;
class Sender;
}

namespace net {
class HostResolver;
}

namespace ppapi {
class PpapiPermissions;
namespace host {
class PpapiHost;
}
}

namespace content {

// Interface that allows components in the embedder app to talk to the
// PpapiHost in the browser process.
//
// There will be one of these objects in the browser per plugin process. It
// lives entirely on the I/O thread.
class CONTENT_EXPORT BrowserPpapiHost {
 public:
  // Creates a browser host and sets up an out-of-process proxy for an external
  // pepper plugin process.
  static BrowserPpapiHost* CreateExternalPluginProcess(
      IPC::Sender* sender,
      ppapi::PpapiPermissions permissions,
      base::ProcessHandle plugin_child_process,
      IPC::ChannelProxy* channel,
      net::HostResolver* host_resolver,
      int render_process_id,
      int render_view_id);

  virtual ~BrowserPpapiHost() {}

  // Returns the PpapiHost object.
  virtual ppapi::host::PpapiHost* GetPpapiHost() = 0;

  // Returns the handle to the plugin process.
  virtual base::ProcessHandle GetPluginProcessHandle() const = 0;

  // Returns true if the given PP_Instance is valid.
  virtual bool IsValidInstance(PP_Instance instance) const = 0;

  // Retrieves the process/view Ids associated with the RenderView containing
  // the given instance and returns true on success. If the instance is
  // invalid, the ids will be 0 and false will be returned.
  //
  // When a resource is created, the PP_Instance should already have been
  // validated, and the resource hosts will be deleted when the resource is
  // destroyed. So it should not generally be necessary to check for errors
  // from this function except as a last-minute sanity check if you convert the
  // IDs to a RenderView/ProcessHost on the UI thread.
  virtual bool GetRenderViewIDsForInstance(PP_Instance instance,
                                           int* render_process_id,
                                           int* render_view_id) const = 0;

  // Returns the name of the plugin.
  virtual const std::string& GetPluginName() = 0;

  // Returns the user's profile data directory.
  virtual const FilePath& GetProfileDataDirectory() = 0;

  // Get the Document/Plugin URLs for the given PP_Instance.
  virtual GURL GetDocumentURLForInstance(PP_Instance instance) = 0;
  virtual GURL GetPluginURLForInstance(PP_Instance instance) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PPAPI_HOST_H_
