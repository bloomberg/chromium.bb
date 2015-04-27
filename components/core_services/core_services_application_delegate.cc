// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/core_services/core_services_application_delegate.h"

#include "components/clipboard/clipboard_standalone_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_connection.h"

namespace core_services {

CoreServicesApplicationDelegate::CoreServicesApplicationDelegate() {}

CoreServicesApplicationDelegate::~CoreServicesApplicationDelegate() {}

bool CoreServicesApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  // TODO(erg): For version one, we'll just say that all incoming connections
  // get access to the same objects, which imply that there will be one
  // instance of the service in its own process. However, in the long run,
  // we'll want this to be more configurable. Some services are singletons,
  // while we'll want to spawn a new process with multiple instances for other
  // services.
  connection->AddService<mojo::ServiceProvider>(this);
  return true;
}

void CoreServicesApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<ServiceProvider> request) {
  provider_bindings_.AddBinding(this, request.Pass());
}

void CoreServicesApplicationDelegate::ConnectToService(
    const mojo::String& service_name,
    mojo::ScopedMessagePipeHandle client_handle) {
  if (service_name == mojo::Clipboard::Name_) {
    // TODO(erg): So what we do here probably doesn't look like the
    // InProcessNativeRunner / NativeApplicationSupport.
    // native_application_support.cc does the whole SetThunks() stuff. This has
    // already happened since Core Services is a mojo application. So we want
    // some sort of lightweight runner here.
    //
    // But...the actual child process stuff is its own mojom! (Also, it's
    // entangled with mojo::Shell::ChildProcessMain().) All concept of app
    // paths are the things which are used to execute the application in
    // child_process.cc.

    // TODO(erg): The lifetime of ClipboardStandaloneImpl is wrong. Right now,
    // a new object is made for each request, but we obviously want there to be
    // one clipboard across all callers.
    new clipboard::ClipboardStandaloneImpl(
        mojo::MakeRequest<mojo::Clipboard>(client_handle.Pass()));
  }
}

}  // namespace core_services
