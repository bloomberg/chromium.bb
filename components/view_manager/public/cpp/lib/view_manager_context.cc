// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/view_manager_context.h"

#include "mojo/application/public/cpp/application_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace mojo {
class ApplicationImpl;

class ViewManagerContext::InternalState {
 public:
  explicit InternalState(ApplicationImpl* application_impl) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:window_manager");
    application_impl->ConnectToService(request.Pass(), &wm_);
  }
  ~InternalState() {}

  WindowManager* wm() { return wm_.get(); }

 private:
  WindowManagerPtr wm_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(InternalState);
};

ViewManagerContext::ViewManagerContext(ApplicationImpl* application_impl)
    : state_(new InternalState(application_impl)) {}
ViewManagerContext::~ViewManagerContext() {
  delete state_;
}

void ViewManagerContext::Embed(const String& url) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(url);
  Embed(request.Pass(), nullptr, nullptr);
}

void ViewManagerContext::Embed(mojo::URLRequestPtr request,
                               InterfaceRequest<ServiceProvider> services,
                               ServiceProviderPtr exposed_services) {
  state_->wm()->Embed(request.Pass(), services.Pass(), exposed_services.Pass());
}

}  // namespace mojo
