// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/package_manager/content_handler_connection.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/identity.h"

namespace mojo {
namespace shell {

ContentHandlerConnection::ContentHandlerConnection(
    ApplicationManager* manager,
    const Identity& source,
    const Identity& content_handler,
    uint32_t id,
    const ClosedCallback& connection_closed_callback)
    : connection_closed_callback_(connection_closed_callback),
      identity_(content_handler),
      connection_closed_(false),
      id_(id),
      ref_count_(0) {
  ServiceProviderPtr services;

  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->set_source(source);
  params->SetTarget(identity_);
  params->set_services(GetProxy(&services));
  manager->ConnectToApplication(std::move(params));

  MessagePipe pipe;
  content_handler_.Bind(
      InterfacePtrInfo<mojom::ContentHandler>(std::move(pipe.handle0), 0u));
  services->ConnectToService(mojom::ContentHandler::Name_,
                             std::move(pipe.handle1));
  content_handler_.set_connection_error_handler(
      [this]() { CloseConnection(); });
}

void ContentHandlerConnection::StartApplication(
    InterfaceRequest<mojom::ShellClient> request,
    URLResponsePtr response) {
  content_handler_->StartApplication(
      std::move(request), std::move(response),
      base::Bind(&ContentHandlerConnection::ApplicationDestructed,
                 base::Unretained(this)));
  ref_count_++;
}

void ContentHandlerConnection::CloseConnection() {
  if (connection_closed_)
    return;
  connection_closed_ = true;
  connection_closed_callback_.Run(this);
  delete this;
}

ContentHandlerConnection::~ContentHandlerConnection() {
  // If this DCHECK fails then something has tried to delete this object without
  // calling CloseConnection.
  DCHECK(connection_closed_);
}

void ContentHandlerConnection::ApplicationDestructed() {
  if (!--ref_count_)
    CloseConnection();
}

}  // namespace shell
}  // namespace mojo
