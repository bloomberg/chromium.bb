// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/run.h"

#include "base/logging.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/shell/context.h"

namespace mojo {
namespace shell {

class StubServiceProvider : public InterfaceImpl<ServiceProvider> {
 private:
  virtual void ConnectToService(const mojo::String& service_name,
                                ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE {
  }
};


void Run(Context* context, const std::vector<GURL>& app_urls) {
  if (app_urls.empty()) {
    LOG(ERROR) << "No app path specified";
    return;
  }

  for (std::vector<GURL>::const_iterator it = app_urls.begin();
       it != app_urls.end();
       ++it) {
    // TODO(davemoore): These leak...need refs to them.
    StubServiceProvider* stub_sp = new StubServiceProvider;
    ServiceProviderPtr spp;
    BindToProxy(stub_sp, &spp);

    context->application_manager()->ConnectToApplication(
        *it, GURL(), spp.Pass());
  }
}

}  // namespace shell
}  // namespace mojo
