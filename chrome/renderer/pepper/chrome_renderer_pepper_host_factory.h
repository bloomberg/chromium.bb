// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_CHROME_RENDERER_PEPPER_HOST_FACTORY_H_
#define CHROME_RENDERER_PEPPER_CHROME_RENDERER_PEPPER_HOST_FACTORY_H_

#include "base/compiler_specific.h"
#include "ppapi/host/host_factory.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace chrome {

class PepperInstanceStateAccessor;

class ChromeRendererPepperHostFactory : public ppapi::host::HostFactory {
 public:
  ChromeRendererPepperHostFactory();
  virtual ~ChromeRendererPepperHostFactory();

  // HostFactory.
  virtual scoped_ptr<ppapi::host::ResourceHost> CreateResourceHost(
      ppapi::host::PpapiHost* host,
      const ppapi::proxy::ResourceMessageCallParams& params,
      PP_Instance instance,
      const IPC::Message& message) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeRendererPepperHostFactory);
};

}  // namespace chrome

#endif  // CHROME_RENDERER_PEPPER_CHROME_RENDERER_PEPPER_HOST_FACTORY_H_
