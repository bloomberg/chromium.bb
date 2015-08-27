// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/view_tree_host_connection.h"

#include "components/view_manager/public/cpp/lib/view_tree_client_impl.h"
#include "components/view_manager/public/cpp/view_tree_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace mojo {

ViewTreeHostConnection::ViewTreeHostConnection(
    ApplicationImpl* app,
    ViewTreeDelegate* delegate,
    ViewTreeHostClient* host_client) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:view_manager");

  ViewTreeHostFactoryPtr factory;
  app->ConnectToService(request.Pass(), &factory);

  ViewTreeHostClientPtr host_client_ptr;
  if (host_client) {
    host_client_binding_.reset(new Binding<ViewTreeHostClient>(host_client));
    host_client_binding_->Bind(GetProxy(&host_client_ptr));
  }

  ViewTreeClientPtr tree_client_ptr;
  ViewTreeConnection::Create(delegate, GetProxy(&tree_client_ptr));

  factory->CreateViewTreeHost(GetProxy(&host_),
                              host_client_ptr.Pass(),
                              tree_client_ptr.Pass());
}

ViewTreeHostConnection::~ViewTreeHostConnection() {}

}  // namespace mojo
