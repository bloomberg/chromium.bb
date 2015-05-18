// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/view_manager_init.h"

#include "components/view_manager/public/cpp/lib/view_manager_client_impl.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace mojo {

class ViewManagerInit::ClientFactory
    : public InterfaceFactory<ViewManagerClient> {
 public:
  ClientFactory(ViewManagerInit* init) : init_(init) {}
  ~ClientFactory() override {}

  // InterfaceFactory<ViewManagerClient> implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ViewManagerClient> request) override {
    init_->OnCreate(request.Pass());
  }

 private:
  ViewManagerInit* init_;

  DISALLOW_COPY_AND_ASSIGN(ClientFactory);
};

ViewManagerInit::ViewManagerInit(ApplicationImpl* app,
                                 ViewManagerDelegate* delegate,
                                 ViewManagerRootClient* root_client)
    : app_(app), delegate_(delegate), client_factory_(new ClientFactory(this)) {
  ApplicationConnection* connection =
      app_->ConnectToApplication("mojo:view_manager");
  connection->AddService(client_factory_.get());
  connection->ConnectToService(&service_);
  service_.set_error_handler(this);
  connection->ConnectToService(&view_manager_root_);
  if (root_client) {
    root_client_binding_.reset(new Binding<ViewManagerRootClient>(root_client));
    ViewManagerRootClientPtr root_client_ptr;
    root_client_binding_->Bind(GetProxy(&root_client_ptr));
    view_manager_root_->SetViewManagerRootClient(root_client_ptr.Pass());
  }
}

ViewManagerInit::~ViewManagerInit() {
}

void ViewManagerInit::OnCreate(InterfaceRequest<ViewManagerClient> request) {
  // TODO(sky): straighten out lifetime.
  const bool delete_on_error = false;
  ViewManagerClientImpl* client = new ViewManagerClientImpl(
      delegate_, app_->shell(), request.Pass(), delete_on_error);
  client->SetViewManagerService(service_.Pass());
}

void ViewManagerInit::OnConnectionError() {
  mojo::ApplicationImpl::Terminate();
}

}  // namespace mojo
