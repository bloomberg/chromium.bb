// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/browser/renderer_host/pepper/content_browser_pepper_host_factory.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ppapi/host/ppapi_host.h"

namespace IPC {
class Sender;
}

namespace content {

class BrowserPpapiHostImpl : public BrowserPpapiHost,
                             public IPC::ChannelProxy::MessageFilter {
 public:
  BrowserPpapiHostImpl(IPC::Sender* sender,
                       const ppapi::PpapiPermissions& permissions);

  // IPC::ChannelProxy::MessageFilter.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // BrowserPpapiHost.
  virtual ppapi::host::PpapiHost* GetPpapiHost() OVERRIDE;

 private:
  virtual ~BrowserPpapiHostImpl();

  ContentBrowserPepperHostFactory host_factory_;
  ppapi::host::PpapiHost ppapi_host_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPpapiHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_
