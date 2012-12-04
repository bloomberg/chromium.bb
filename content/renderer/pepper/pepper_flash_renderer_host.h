// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FLASH_RENDERER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FLASH_RENDERER_HOST_H_

#include "base/basictypes.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {

class RendererPpapiHost;

class PepperFlashRendererHost : public ppapi::host::ResourceHost {
 public:
  PepperFlashRendererHost(RendererPpapiHost* host,
                          PP_Instance instance,
                          PP_Resource resource);
  virtual ~PepperFlashRendererHost();

  // ppapi::host::ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  int32_t OnMsgGetProxyForURL(ppapi::host::HostMessageContext* host_context,
                              const std::string& url);

  DISALLOW_COPY_AND_ASSIGN(PepperFlashRendererHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FLASH_RENDERER_HOST_H_
