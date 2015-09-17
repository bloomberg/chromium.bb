// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/view_tree_host_factory.h"

#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace mus {

void CreateViewTreeHost(mojo::ViewTreeHostFactory* factory,
                        mojo::ViewTreeHostClientPtr host_client,
                        ViewTreeDelegate* delegate,
                        mojo::ViewTreeHostPtr* host) {
  mojo::ViewTreeClientPtr tree_client;
  ViewTreeConnection::Create(delegate, GetProxy(&tree_client));
  factory->CreateViewTreeHost(GetProxy(host), host_client.Pass(),
                              tree_client.Pass());
}

void CreateSingleViewTreeHost(mojo::ApplicationImpl* app,
                              ViewTreeDelegate* delegate,
                              mojo::ViewTreeHostPtr* host) {
  mojo::ViewTreeHostFactoryPtr factory;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:mus";
  app->ConnectToService(request.Pass(), &factory);
  CreateViewTreeHost(factory.get(), mojo::ViewTreeHostClientPtr(), delegate,
                     host);
}

}  // namespace mus
