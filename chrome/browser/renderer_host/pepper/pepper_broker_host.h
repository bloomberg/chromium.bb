// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_BROKER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_BROKER_HOST_H_

#include "ppapi/host/resource_host.h"

namespace content {
class BrowserPpapiHost;
}

namespace chrome {

class PepperBrokerHost : public ppapi::host::ResourceHost {
 public:
  PepperBrokerHost(content::BrowserPpapiHost* host,
                   PP_Instance instance,
                   PP_Resource resource);
  virtual ~PepperBrokerHost();

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperBrokerHost);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_BROKER_HOST_H_
