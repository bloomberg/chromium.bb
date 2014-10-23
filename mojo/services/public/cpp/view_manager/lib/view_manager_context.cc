// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_manager_context.h"

#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/services/public/interfaces/window_manager/window_manager.mojom.h"

namespace mojo {
class ApplicationImpl;

class ViewManagerContext::InternalState {
 public:
  explicit InternalState(ApplicationImpl* application_impl) {
    application_impl->ConnectToService("mojo:window_manager", &wm_);
  }
  ~InternalState() {}

  WindowManager* wm() { return wm_.get(); }

 private:
  WindowManagerPtr wm_;

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
  state_->wm()->Embed(url, MakeRequest<ServiceProvider>(sp.PassMessagePipe()));
  return imported_services.Pass();
}

}  // namespace mojo
