// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window_tree_host_factory.h"

#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace mus {

void CreateWindowTreeHost(mojo::ViewTreeHostFactory* factory,
                          mojo::ViewTreeHostClientPtr host_client,
                          WindowTreeDelegate* delegate,
                          mojo::ViewTreeHostPtr* host) {
  mojo::ViewTreeClientPtr tree_client;
  WindowTreeConnection::Create(
      delegate, GetProxy(&tree_client),
      WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
  factory->CreateWindowTreeHost(GetProxy(host), host_client.Pass(),
                                tree_client.Pass());
}

void CreateSingleWindowTreeHost(mojo::ApplicationImpl* app,
                                WindowTreeDelegate* delegate,
                                mojo::ViewTreeHostPtr* host) {
  mojo::ViewTreeHostFactoryPtr factory;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:mus";
  app->ConnectToService(request.Pass(), &factory);
  CreateWindowTreeHost(factory.get(), mojo::ViewTreeHostClientPtr(), delegate,
                       host);
}

}  // namespace mus
