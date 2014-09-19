// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_APPLICATION_APPLICATION_IMPL_H_
#define MOJO_PUBLIC_APPLICATION_APPLICATION_IMPL_H_
#include <vector>

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/lib/service_connector.h"
#include "mojo/public/cpp/application/lib/service_registry.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/application/application.mojom.h"
#include "mojo/public/interfaces/application/shell.mojom.h"

namespace mojo {

class ApplicationDelegate;

// Utility class for communicating with the Shell, and providing Services
// to clients.
//
// To use define a class that implements your specific server api, e.g. FooImpl
// to implement a service named Foo.
// That class must subclass an InterfaceImpl specialization.
//
// If there is context that is to be shared amongst all instances, define a
// constructor with that class as its only argument, otherwise define an empty
// constructor.
//
// class FooImpl : public InterfaceImpl<Foo> {
//  public:
//   FooImpl(ApplicationContext* app_context) {}
// };
//
// or
//
// class BarImpl : public InterfaceImpl<Bar> {
//  public:
//   // contexts will remain valid for the lifetime of BarImpl.
//   BarImpl(ApplicationContext* app_context, BarContext* service_context)
//          : app_context_(app_context), servicecontext_(context) {}
//
// Create an ApplicationImpl instance that collects any service implementations.
//
// ApplicationImpl app(service_provider_handle);
// app.AddService<FooImpl>();
//
// BarContext context;
// app.AddService<BarImpl>(&context);
//
//
class ApplicationImpl : public InterfaceImpl<Application> {
 public:
  ApplicationImpl(ApplicationDelegate* delegate,
                  ScopedMessagePipeHandle shell_handle);
  ApplicationImpl(ApplicationDelegate* delegate,
                  MojoHandle shell_handle);
  virtual ~ApplicationImpl();

  Shell* shell() const { return shell_.get(); }

  // Returns any initial configuration arguments, passed by the Shell.
  const Array<String>& args() { return args_; }

  // Establishes a new connection to an application. Caller does not own.
  ApplicationConnection* ConnectToApplication(const String& application_url);

  // Connect to application identified by |application_url| and connect to the
  // service implementation of the interface identified by |Interface|.
  template <typename Interface>
  void ConnectToService(const std::string& application_url,
                        InterfacePtr<Interface>* ptr) {
    ConnectToApplication(application_url)->ConnectToService(ptr);
  }

 private:
  class ShellPtrWatcher;

  void BindShell(ScopedMessagePipeHandle shell_handle);
  void ClearConnections();
  void OnShellError() {
    ClearConnections();
    Terminate();
  };

  // Quits the main run loop for this application.
  static void Terminate();

  // Application implementation.
  virtual void Initialize(Array<String> args) MOJO_OVERRIDE;
  virtual void AcceptConnection(const String& requestor_url,
                                ServiceProviderPtr provider) MOJO_OVERRIDE;

  typedef std::vector<internal::ServiceRegistry*> ServiceRegistryList;

  bool initialized_;
  ServiceRegistryList incoming_service_registries_;
  ServiceRegistryList outgoing_service_registries_;
  ApplicationDelegate* delegate_;
  ShellPtr shell_;
  ShellPtrWatcher* shell_watch_;
  Array<String> args_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ApplicationImpl);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_APPLICATION_APPLICATION_IMPL_H_
