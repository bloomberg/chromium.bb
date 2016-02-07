// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_APPLICATION_IMPL_H_
#define MOJO_SHELL_PUBLIC_CPP_APPLICATION_IMPL_H_

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/shell/public/cpp/app_lifetime_helper.h"
#include "mojo/shell/public/cpp/lib/service_registry.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/application.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace mojo {

// TODO(beng): This comment is hilariously out of date.
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
class ApplicationImpl : public Shell, public shell::mojom::Application {
 public:
  class TestApi {
   public:
    explicit TestApi(ApplicationImpl* application)
        : application_(application) {}

    void UnbindConnections(
        InterfaceRequest<shell::mojom::Application>* application_request,
        shell::mojom::ShellPtr* shell) {
      application_->UnbindConnections(application_request, shell);
    }

   private:
    ApplicationImpl* application_;
  };

  // Does not take ownership of |delegate|, which must remain valid for the
  // lifetime of ApplicationImpl.
  ApplicationImpl(ShellClient* client,
                  InterfaceRequest<shell::mojom::Application> request);
  // Constructs an ApplicationImpl with a custom termination closure. This
  // closure is invoked on Quit() instead of the default behavior of quitting
  // the current base::MessageLoop.
  ApplicationImpl(ShellClient* client,
                  InterfaceRequest<shell::mojom::Application> request,
                  const Closure& termination_closure);
  ~ApplicationImpl() override;

  // The Mojo shell. This will return a valid pointer after Initialize() has
  // been invoked. It will remain valid until UnbindConnections() is invoked or
  // the ApplicationImpl is destroyed.
  shell::mojom::Shell* shell() const { return shell_.get(); }

  // Block the calling thread until the Initialize() method is called by the
  // shell.
  void WaitForInitialize();

  // Shell.
  scoped_ptr<Connection> Connect(const std::string& url) override;
  scoped_ptr<Connection> Connect(ConnectParams* params) override;
  void Quit() override;
  scoped_ptr<AppRefCount> CreateAppRefCount() override;

 private:
  // shell::mojom::Application implementation.
  void Initialize(shell::mojom::ShellPtr shell,
                  const mojo::String& url,
                  uint32_t id) override;
  void AcceptConnection(const String& requestor_url,
                        uint32_t requestor_id,
                        InterfaceRequest<ServiceProvider> services,
                        ServiceProviderPtr exposed_services,
                        Array<String> allowed_interfaces,
                        const String& url) override;
  void OnQuitRequested(const Callback<void(bool)>& callback) override;

  void OnConnectionError();

  // Called from Quit() when there is no Shell connection, or asynchronously
  // from Quit() once the Shell has OK'ed shutdown.
  void QuitNow();

  // Unbinds the Shell and Application connections. Can be used to re-bind the
  // handles to another implementation of ApplicationImpl, for instance when
  // running apptests.
  void UnbindConnections(
      InterfaceRequest<shell::mojom::Application>* application_request,
      shell::mojom::ShellPtr* shell);

  // We track the lifetime of incoming connection registries as it more
  // convenient for the client.
  ScopedVector<Connection> incoming_connections_;
  ShellClient* client_;
  Binding<shell::mojom::Application> binding_;
  shell::mojom::ShellPtr shell_;
  Closure termination_closure_;
  AppLifetimeHelper app_lifetime_helper_;
  bool quit_requested_;
  base::WeakPtrFactory<ApplicationImpl> weak_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ApplicationImpl);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_APPLICATION_IMPL_H_
