// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FLASH_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FLASH_HOST_H_

#include "base/basictypes.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace content {

class RendererPpapiHost;

class PepperFlashHost
    : public ppapi::host::ResourceHost {
 public:
  PepperFlashHost(RendererPpapiHost* host,
                  PP_Instance instance,
                  PP_Resource resource);
  virtual ~PepperFlashHost();

  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperFlashHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FLASH_HOST_H_
