// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TALK_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TALK_HOST_H_

#include "base/memory/weak_ptr.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {
class BrowserPpapiHost;
}

namespace ppapi {
namespace host {
struct ReplyMessageContext;
}
}

namespace chrome {

class PepperTalkHost : public ppapi::host::ResourceHost {
 public:
  PepperTalkHost(content::BrowserPpapiHost* host,
                 PP_Instance instance,
                 PP_Resource resource);
  virtual ~PepperTalkHost();

  // ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  // Sends the reply.
  void GotTalkPermission(ppapi::host::ReplyMessageContext reply);

 private:
  base::WeakPtrFactory<PepperTalkHost> weak_factory_;
  content::BrowserPpapiHost* browser_ppapi_host_;

  DISALLOW_COPY_AND_ASSIGN(PepperTalkHost);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TALK_HOST_H_
