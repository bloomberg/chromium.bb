// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PEPPER_CONTENT_BROWSER_PEPPER_HOST_FACTORY_H_
#define CONTENT_BROWSER_PEPPER_CONTENT_BROWSER_PEPPER_HOST_FACTORY_H_

#include "base/compiler_specific.h"
#include "ppapi/host/host_factory.h"

namespace ppapi {
class PpapiPermissions;
}

namespace content {

class BrowserPpapiHostImpl;

class ContentBrowserPepperHostFactory : public ppapi::host::HostFactory {
 public:
  // Non-owning pointer to the filter must outlive this class.
  explicit ContentBrowserPepperHostFactory(BrowserPpapiHostImpl* host);
  virtual ~ContentBrowserPepperHostFactory();

  virtual scoped_ptr<ppapi::host::ResourceHost> CreateResourceHost(
      ppapi::host::PpapiHost* host,
      const ppapi::proxy::ResourceMessageCallParams& params,
      PP_Instance instance,
      const IPC::Message& message) OVERRIDE;

 private:
  const ppapi::PpapiPermissions& GetPermissions() const;

  // Non-owning pointer.
  BrowserPpapiHostImpl* host_;

  DISALLOW_COPY_AND_ASSIGN(ContentBrowserPepperHostFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PEPPER_CONTENT_BROWSER_PEPPER_HOST_FACTORY_H_
