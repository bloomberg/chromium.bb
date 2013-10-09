// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TALK_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TALK_HOST_H_

#include "base/memory/weak_ptr.h"
#include "ppapi/c/private/ppb_talk_private.h"
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

 private:
  // ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  int32_t OnRequestPermission(ppapi::host::HostMessageContext* context,
                              PP_TalkPermission permission);
  int32_t OnStartRemoting(ppapi::host::HostMessageContext* context);
  int32_t OnStopRemoting(ppapi::host::HostMessageContext* context);
  void OnRemotingStopEvent();

  void OnRequestPermissionCompleted(ppapi::host::ReplyMessageContext reply);
  void OnStartRemotingCompleted(ppapi::host::ReplyMessageContext reply);
  void OnStopRemotingCompleted(ppapi::host::ReplyMessageContext reply);

  content::BrowserPpapiHost* browser_ppapi_host_;
  bool remoting_started_;
  base::WeakPtrFactory<PepperTalkHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperTalkHost);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TALK_HOST_H_
