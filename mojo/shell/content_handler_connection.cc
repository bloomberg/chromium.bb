// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/content_handler_connection.h"

#include "base/memory/scoped_ptr.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/identity.h"

namespace mojo {
namespace shell {

ContentHandlerConnection::ContentHandlerConnection(
    ApplicationManager* manager,
    const Identity& originator_identity,
    const CapabilityFilter& originator_filter,
    const GURL& content_handler_url,
    const std::string& qualifier,
    const CapabilityFilter& filter,
    uint32_t id)
    : manager_(manager),
      content_handler_url_(content_handler_url),
      content_handler_qualifier_(qualifier),
      connection_closed_(false),
      id_(id) {
  ServiceProviderPtr services;

  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->set_originator_identity(originator_identity);
  params->set_originator_filter(originator_filter);
  params->SetURLInfo(content_handler_url);
  params->set_qualifier(qualifier);
  params->set_services(GetProxy(&services));
  params->set_filter(filter);

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
  manager_->OnContentHandlerConnectionClosed(this);
  delete this;
}

ContentHandlerConnection::~ContentHandlerConnection() {
  // If this DCHECK fails then something has tried to delete this object without
  // calling CloseConnection.
  DCHECK(connection_closed_);
}

}  // namespace shell
}  // namespace mojo
