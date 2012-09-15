// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/browser/renderer_host/pepper/content_browser_pepper_host_factory.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ppapi/host/ppapi_host.h"

namespace IPC {
class Sender;
}

namespace content {

class CONTENT_EXPORT BrowserPpapiHostImpl
    : public BrowserPpapiHost,
      public IPC::ChannelProxy::MessageFilter {
 public:
  // The creator is responsible for calling set_plugin_process_handle as soon
  // as it is known (we start the process asynchronously so it won't be known
  // when this object is created).
  BrowserPpapiHostImpl(IPC::Sender* sender,
                       const ppapi::PpapiPermissions& permissions);

  // IPC::ChannelProxy::MessageFilter.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // BrowserPpapiHost.
  virtual ppapi::host::PpapiHost* GetPpapiHost() OVERRIDE;
  virtual base::ProcessHandle GetPluginProcessHandle() const OVERRIDE;
  virtual bool IsValidInstance(PP_Instance instance) const OVERRIDE;
  virtual bool GetRenderViewIDsForInstance(PP_Instance instance,
                                           int* render_process_id,
                                           int* render_view_id) const OVERRIDE;

  void set_plugin_process_handle(base::ProcessHandle handle) {
    plugin_process_handle_ = handle;
  }

  // These two functions are notifications that an instance has been created
  // or destroyed. They allow us to maintain a mapping of PP_Instance to view
  // IDs in the browser process.
  void AddInstanceForView(PP_Instance instance,
                          int render_process_id,
                          int render_view_id);
  void DeleteInstanceForView(PP_Instance instance);

 private:
  friend class BrowserPpapiHostTest;

  struct RenderViewIDs {
    int process_id;
    int view_id;
  };
  typedef std::map<PP_Instance, RenderViewIDs> InstanceToViewMap;

  virtual ~BrowserPpapiHostImpl();

  ppapi::host::PpapiHost ppapi_host_;
  base::ProcessHandle plugin_process_handle_;

  // Tracks all PP_Instances in this plugin and maps them to
  // RenderProcess/RenderView IDs.
  InstanceToViewMap instance_to_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPpapiHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_
