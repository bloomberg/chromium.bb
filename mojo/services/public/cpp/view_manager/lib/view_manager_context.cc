// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_manager_context.h"

#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace mojo {
class ApplicationImpl;

void ConnectCallback(bool success) {}

class ViewManagerContext::InternalState {
 public:
  InternalState(ApplicationImpl* application_impl) {
    application_impl->ConnectToService("mojo:mojo_view_manager",
                                       &init_service_);
  }
  ~InternalState() {}

  ViewManagerInitService* init_service() { return init_service_.get(); }

 private:
  ViewManagerInitServicePtr init_service_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(InternalState);
};

ViewManagerContext::ViewManagerContext(ApplicationImpl* application_impl)
    : state_(new InternalState(application_impl)) {}
ViewManagerContext::~ViewManagerContext() {}

void ViewManagerContext::Embed(const String& url) {
  scoped_ptr<ServiceProviderImpl> spi(new ServiceProviderImpl);
  Embed(url, spi.Pass());
}

scoped_ptr<ServiceProvider> ViewManagerContext::Embed(
    const String& url,
    scoped_ptr<ServiceProviderImpl> exported_services) {
  scoped_ptr<ServiceProvider> imported_services;
  // BindToProxy() takes ownership of |exported_services|.
  ServiceProviderImpl* registry = exported_services.release();
  ServiceProviderPtr sp;
  if (registry) {
    BindToProxy(registry, &sp);
    imported_services.reset(registry->CreateRemoteServiceProvider());
  }
  state_->init_service()->Embed(url, sp.Pass(), base::Bind(&ConnectCallback));
  return imported_services.Pass();
}

}  // namespace mojo
