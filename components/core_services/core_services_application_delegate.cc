// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/core_services/core_services_application_delegate.h"

#include "components/clipboard/clipboard_application_delegate.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_connection.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"

namespace core_services {

CoreServicesApplicationDelegate::CoreServicesApplicationDelegate() {}

CoreServicesApplicationDelegate::~CoreServicesApplicationDelegate() {}

bool CoreServicesApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void CoreServicesApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::ContentHandler> request) {
  handler_bindings_.AddBinding(this, request.Pass());
}

void CoreServicesApplicationDelegate::StartApplication(
    mojo::InterfaceRequest<mojo::Application> request,
    mojo::URLResponsePtr response) {
  std::string url = response->url;
  if (url == "mojo://clipboard/") {
    // TODO(erg): This is where we should deal with running things on separate
    // threads.
    clipboard_application_.reset(
        new mojo::ApplicationImpl(new clipboard::ClipboardApplicationDelegate,
                                  request.Pass()));
    return;
  }

  NOTREACHED() << "Unknown service '" << url << "'";
}

}  // namespace core_services
