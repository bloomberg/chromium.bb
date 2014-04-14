// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PEPPER_EXTENSIONS_COMMON_HOST_H_
#define CHROME_RENDERER_PEPPER_PEPPER_EXTENSIONS_COMMON_HOST_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/renderer/extensions/pepper_request_proxy.h"
#include "ppapi/host/resource_host.h"

namespace base {
class ListValue;
}

namespace content {
class RendererPpapiHost;
}

namespace ppapi {
namespace host {
struct ReplyMessageContext;
}
}

namespace extensions {
class Dispatcher;
}

class PepperExtensionsCommonHost : public ppapi::host::ResourceHost {
 public:
  virtual ~PepperExtensionsCommonHost();

  static PepperExtensionsCommonHost* Create(content::RendererPpapiHost* host,
                                            PP_Instance instance,
                                            PP_Resource resource);

  // ppapi::host::ResourceMessageHandler overrides.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  PepperExtensionsCommonHost(
      content::RendererPpapiHost* host,
      PP_Instance instance,
      PP_Resource resource,
      extensions::PepperRequestProxy* pepper_request_proxy);

  int32_t OnPost(ppapi::host::HostMessageContext* context,
                 const std::string& request_name,
                 const base::ListValue& args);

  int32_t OnCall(ppapi::host::HostMessageContext* context,
                 const std::string& request_name,
                 const base::ListValue& args);

  void OnResponseReceived(ppapi::host::ReplyMessageContext reply_context,
                          bool success,
                          const base::ListValue& response,
                          const std::string& error);

  // Non-owning pointer.
  content::RendererPpapiHost* renderer_ppapi_host_;
  // Non-owning pointer.
  extensions::PepperRequestProxy* pepper_request_proxy_;

  base::WeakPtrFactory<PepperExtensionsCommonHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperExtensionsCommonHost);
};

#endif  // CHROME_RENDERER_PEPPER_PEPPER_EXTENSIONS_COMMON_HOST_H_
