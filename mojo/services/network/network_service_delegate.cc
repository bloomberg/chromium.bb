// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_service_delegate.h"

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/services/network/network_service_impl.h"
#include "mojo/services/network/url_loader_factory_impl.h"

NetworkServiceDelegate::NetworkServiceDelegate() : app_(nullptr) {}

NetworkServiceDelegate::~NetworkServiceDelegate() {}

void NetworkServiceDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
  base::FilePath base_path;
  CHECK(PathService::Get(base::DIR_TEMP, &base_path));
  base_path = base_path.Append(FILE_PATH_LITERAL("network_service"));
  context_.reset(new mojo::NetworkContext(base_path));
}

bool NetworkServiceDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  DCHECK(context_);
  connection->AddService<mojo::NetworkService>(this);
  connection->AddService<mojo::URLLoaderFactory>(this);
  return true;
}

void NetworkServiceDelegate::Quit() {
  // Destroy the NetworkContext now as it requires MessageLoop::current() upon
  // destruction and it is the last moment we know for sure that it is
  // running.
  context_.reset();
}

void NetworkServiceDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::NetworkService> request) {
  new mojo::NetworkServiceImpl(
      connection,
      context_.get(),
      app_->app_lifetime_helper()->CreateAppRefCount(),
      request.Pass());
}

void NetworkServiceDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::URLLoaderFactory> request) {
  new mojo::URLLoaderFactoryImpl(
      connection,
      context_.get(),
      app_->app_lifetime_helper()->CreateAppRefCount(),
      request.Pass());
}
