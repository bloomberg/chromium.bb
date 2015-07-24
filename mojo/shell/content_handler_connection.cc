// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/content_handler_connection.h"

#include "mojo/shell/application_manager.h"

namespace mojo {
namespace shell {

ContentHandlerConnection::ContentHandlerConnection(
    ApplicationManager* manager,
    const GURL& content_handler_url,
    const GURL& requestor_url,
    const std::string& qualifier)
    : manager_(manager),
      content_handler_url_(content_handler_url),
      content_handler_qualifier_(qualifier),
      connection_closed_(false) {
  ServiceProviderPtr services;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(content_handler_url.spec());
  // TODO(beng): Need to figure out how to deal with originator and
  //             CapabilityFilter here.
  manager->ConnectToApplication(
      nullptr, request.Pass(), qualifier, requestor_url, GetProxy(&services),
      nullptr, nullptr, base::Closure());
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
