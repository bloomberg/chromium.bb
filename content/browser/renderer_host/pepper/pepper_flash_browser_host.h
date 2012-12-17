// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FLASH_BROWSER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FLASH_BROWSER_HOST_H_

#include "base/basictypes.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace base {
class Time;
}

namespace content {

class BrowserPpapiHost;

class PepperFlashBrowserHost : public ppapi::host::ResourceHost {
 public:
  PepperFlashBrowserHost(BrowserPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource);
  virtual ~PepperFlashBrowserHost();

  // ppapi::host::ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  int32_t OnMsgUpdateActivity(ppapi::host::HostMessageContext* host_context);
  int32_t OnMsgGetLocalTimeZoneOffset(
      ppapi::host::HostMessageContext* host_context,
      const base::Time& t);

  DISALLOW_COPY_AND_ASSIGN(PepperFlashBrowserHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FLASH_BROWSER_HOST_H_
