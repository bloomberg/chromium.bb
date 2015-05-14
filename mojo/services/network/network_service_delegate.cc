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

NetworkServiceDelegate::NetworkServiceDelegate() {}

NetworkServiceDelegate::~NetworkServiceDelegate() {}

void NetworkServiceDelegate::Initialize(mojo::ApplicationImpl* app) {
  base::FilePath base_path;
  CHECK(PathService::Get(base::DIR_TEMP, &base_path));
  base_path = base_path.Append(FILE_PATH_LITERAL("network_service"));
  context_.reset(new mojo::NetworkContext(base_path));
}

bool NetworkServiceDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  DCHECK(context_);
  connection->AddService(this);
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
  mojo::BindToRequest(
      new mojo::NetworkServiceImpl(connection, context_.get()), &request);
}
