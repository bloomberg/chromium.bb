// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_APPLICATION_APPLICATION_CONNECTION_H_
#define MOJO_PUBLIC_APPLICATION_APPLICATION_CONNECTION_H_

#include <string>

#include "mojo/public/cpp/application/lib/service_connector.h"

namespace mojo {

// An instance of this class is passed to
// ApplicationDelegate's ConfigureIncomingConnection() method each time a
// connection is made to this app, and to ApplicationDelegate's
// ConfigureOutgoingConnection() method when the app connects to
// another.
//
// To use define a class that implements your specific service api, e.g. FooImpl
// to implement a service named Foo.
// That class must subclass an InterfaceImpl specialization.
//
// If there is context that is to be shared amongst all instances, define a
// constructor with that class as its only argument, otherwise define an empty
// constructor.
//
// class FooImpl : public InterfaceImpl<Foo> {
//  public:
//   explicit FooImpl(ApplicationConnnection* connection) {}
// };
//
// or
//
// class BarImpl : public InterfaceImpl<Bar> {
//  public:
//   // contexts will remain valid for the lifetime of BarImpl.
//   BarImpl(ApplicationConnnection* connection, BarContext* service_context)
//          : connection_(connection), servicecontext_(context) {}
//
// Create an ApplicationDelegate instance and pass it to the constructor
// of an ApplicationImpl. The delegate will be called when new connections are
// made to other applications.
//
// connection->AddService<FooImpl>();
//
// BarContext context;
// connection->AddService<BarImpl>(&context);
class ApplicationConnection {
 public:
  virtual ~ApplicationConnection();

  // Impl’s constructor will receive two arguments:
  // Impl::Impl(Application::Context* app_context,
  //            ServiceContext* svc_context)
  template <typename Impl, typename ServiceContext>
  void AddService(ServiceContext* context) {
    AddServiceConnector(
        new internal::ServiceConnector<Impl, ServiceContext>(Impl::Name_,
                                                             context));
  }

  // Impl’s constructor will receive one argument:
  // Impl::Impl(Application::Context* app_context)
  template <typename Impl>
  void AddService() {
    AddServiceConnector(
        new internal::ServiceConnector<Impl, void>(Impl::Name_, NULL));
  }

  // Connect to the service implementing |Interface|.
  template <typename Interface>
  void ConnectToService(InterfacePtr<Interface>* ptr) {
    MessagePipe pipe;
    ptr->Bind(pipe.handle0.Pass());
    GetServiceProvider()->ConnectToService(Interface::Name_,
                                           pipe.handle1.Pass());
  }

  // The url identifying the application on the other end of this connection.
  virtual const std::string& GetRemoteApplicationURL() = 0;

  // Establishes a new connection to an application.
  // TODO(davemoore): Would it be better to expose the ApplicationImpl?
  virtual ApplicationConnection* ConnectToApplication(
      const std::string& url) = 0;

  // Connect to application identified by |application_url| and connect to
  // the service implementation of the interface identified by |Interface|.
  template <typename Interface>
  void ConnectToService(const std::string& application_url,
                        InterfacePtr<Interface>* ptr) {
    ConnectToApplication(application_url)->ConnectToService(ptr);
  }

  // Raw ServiceProvider interface to remote application.
  virtual ServiceProvider* GetServiceProvider() = 0;

private:
 virtual void AddServiceConnector(
      internal::ServiceConnectorBase* service_connector) = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_APPLICATION_APPLICATION_CONNECTION_H_
