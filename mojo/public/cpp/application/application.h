// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_APPLICATION_APPLICATION_H_
#define MOJO_PUBLIC_APPLICATION_APPLICATION_H_
#include <vector>

#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/lib/service_connector.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define APPLICATION_EXPORT __declspec(dllexport)
#else
#define CDECL
#define APPLICATION_EXPORT __attribute__((visibility("default")))
#endif

// DSOs can either implement MojoMain directly or utilize the
// mojo_main_{standalone|chromium} gyp targets and implement
// Application::Create();
// TODO(davemoore): Establish this as part of our SDK for third party mojo
// application writers.
extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle service_provider_handle);

namespace mojo {

// Utility class for creating ServiceProviders that vend service instances.
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
//   FooImpl() {}
// };
//
// or
//
// class BarImpl : public InterfaceImpl<Bar> {
//  public:
//   // context will remain valid for the lifetime of BarImpl.
//   BarImpl(BarContext* context) : context_(context) {}
//  private:
//   BarContext* context;
// };
//
// Create an Application instance that collects any service implementations.
//
// Application app(service_provider_handle);
// app.AddService<FooImpl>();
//
// BarContext context;
// app.AddService<BarImpl>(&context);
//
//
class Application : public internal::ServiceConnectorBase::Owner {
 public:
  Application();
  explicit Application(ScopedMessagePipeHandle service_provider_handle);
  explicit Application(MojoHandle service_provider_handle);
  virtual ~Application();

  // Override this to do any necessary initialization. There's no need to call
  // Application's implementation.
  // The service_provider will be bound to its pipe before this is called.
  virtual void Initialize();

  template <typename Impl, typename Context>
  void AddService(Context* context) {
    AddServiceConnector(new internal::ServiceConnector<Impl, Context>(context));
  }

  template <typename Impl>
  void AddService() {
    AddServiceConnector(new internal::ServiceConnector<Impl, void>(NULL));
  }

  template <typename Interface>
  void ConnectTo(const std::string& url,
                 InterfacePtr<Interface>* ptr) {
    mojo::ConnectToService(service_provider(), url, ptr);
  }

  ServiceProvider* service_provider() { return service_provider_.get(); }
  void BindServiceProvider(ScopedMessagePipeHandle service_provider_handle);

 protected:
  // ServiceProvider methods.
  // Override this to dispatch to correct service when there's more than one.
  // TODO(davemoore): Augment this with name registration.
  virtual void ConnectToService(const mojo::String& url,
                                ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE;

 private:
  friend MojoResult (::MojoMain)(MojoHandle);

  // Implement this method to create the specific subclass of Application.
  static Application* Create();

  // internal::ServiceConnectorBase::Owner methods.
  // Takes ownership of |service_connector|.
  virtual void AddServiceConnector(
      internal::ServiceConnectorBase* service_connector) MOJO_OVERRIDE;
  virtual void RemoveServiceConnector(
      internal::ServiceConnectorBase* service_connector) MOJO_OVERRIDE;

  typedef std::vector<internal::ServiceConnectorBase*> ServiceConnectorList;
  ServiceConnectorList service_connectors_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_APPLICATION_APPLICATION_H_
