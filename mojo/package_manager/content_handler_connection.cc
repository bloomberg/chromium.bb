// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/package_manager/content_handler_connection.h"

#include "base/memory/scoped_ptr.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/identity.h"

namespace mojo {
namespace package_manager {

ContentHandlerConnection::ContentHandlerConnection(
    shell::ApplicationManager* manager,
    const shell::Identity& source,
    const shell::Identity& content_handler,
    uint32_t id,
    const ClosedCallback& connection_closed_callback)
    : connection_closed_callback_(connection_closed_callback),
      identity_(content_handler),
      connection_closed_(false),
      id_(id) {
  ServiceProviderPtr services;

  scoped_ptr<shell::ConnectToApplicationParams> params(
      new shell::ConnectToApplicationParams);
  params->set_source(source);
  params->SetTarget(identity_);
  params->set_services(GetProxy(&services));
  manager->ConnectToApplication(params.Pass());

  MessagePipe pipe;
  content_handler_.Bind(
      InterfacePtrInfo<ContentHandler>(pipe.handle0.Pass(), 0u));
  services->ConnectToService(ContentHandler::Name_, pipe.handle1.Pass());
  content_handler_.set_connection_error_handler(
      [this]() { CloseConnection(); });
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

}  // namespace package_manager
}  // namespace mojo
