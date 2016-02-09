// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_CONNECTION_H_
#define MOJO_SHELL_PUBLIC_CPP_CONNECTION_H_

#include <stdint.h>

#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "mojo/shell/public/cpp/lib/interface_factory_binder.h"
#include "mojo/shell/public/interfaces/service_provider.mojom.h"

namespace mojo {

class InterfaceBinder;

// Represents a connection to another application. An instance of this class is
// passed to ShellClient's AcceptConnection() method each
// time a connection is made to this app.
//
// To use, define a class that implements your specific service API (e.g.,
// FooImpl to implement a service named Foo). Then implement an
// InterfaceFactory<Foo> that binds instances of FooImpl to
// InterfaceRequest<Foo>s and register that on the connection like this:
//
//   connection->AddService(&factory);
//
// Or, if you have multiple factories implemented by the same type, explicitly
// specify the interface to register the factory for:
//
//   connection->AddService<Foo>(&my_foo_and_bar_factory_);
//   connection->AddService<Bar>(&my_foo_and_bar_factory_);
//
// The InterfaceFactory must outlive the Connection.
//
// Additionally you specify a InterfaceBinder. If a InterfaceBinder has
// been set and an InterfaceFactory has not been registered for the interface
// request, than the interface request is sent to the InterfaceBinder.
//
// Just as with InterfaceFactory, InterfaceBinder must outlive Connection.
//
// An Connection's lifetime is managed by an ShellConnection. To close a
// connection, call CloseConnection which will destroy this object.
class Connection {
 public:
  virtual ~Connection() {}

  class TestApi {
   public:
    explicit TestApi(Connection* connection) : connection_(connection) {}
    base::WeakPtr<Connection> GetWeakPtr() {
      return connection_->GetWeakPtr();
    }

   private:
    Connection* connection_;
  };

  // See class description for details.
  virtual void SetDefaultInterfaceBinder(InterfaceBinder* binder) = 0;

  // Makes Interface available as a service to the remote application.
  // |factory| will create implementations of Interface on demand.
  // Returns true if the service was exposed, false if capability filtering
  // from the shell prevented the service from being exposed.
  template <typename Interface>
  bool AddService(InterfaceFactory<Interface>* factory) {
    return SetInterfaceBinderForName(
        new internal::InterfaceFactoryBinder<Interface>(factory),
        Interface::Name_);
  }

  // Binds |ptr| to an implemention of Interface in the remote application.
  // |ptr| can immediately be used to start sending requests to the remote
  // service.
  template <typename Interface>
  void ConnectToService(InterfacePtr<Interface>* ptr) {
    if (ServiceProvider* sp = GetServiceProvider()) {
      MessagePipe pipe;
      ptr->Bind(InterfacePtrInfo<Interface>(std::move(pipe.handle0), 0u));
      sp->ConnectToService(Interface::Name_, std::move(pipe.handle1));
    }
  }

  // Returns the URL that was used by the source application to establish a
  // connection to the destination application.
  //
  // When Connection is representing an incoming connection this can be
  // different than the URL the application was initially loaded from, if the
  // application handles multiple URLs. Note that this is the URL after all
  // URL rewriting and HTTP redirects have been performed.
  //
  // When Connection is representing and outgoing connection, this will be the
  // same as the value returned by GetRemoveApplicationURL().
  virtual const std::string& GetConnectionURL() = 0;

  // Returns the URL identifying the remote application on this connection.
  virtual const std::string& GetRemoteApplicationURL() = 0;

  // Returns the raw proxy to the remote application's ServiceProvider
  // interface. Most applications will just use ConnectToService() instead.
  // Caller does not take ownership.
  virtual ServiceProvider* GetServiceProvider() = 0;

  // Returns the local application's ServiceProvider interface. The return
  // value is owned by this connection.
  virtual ServiceProvider* GetLocalServiceProvider() = 0;

  // Register a handler to receive an error notification on the pipe to the
  // remote application's service provider.
  virtual void SetRemoteServiceProviderConnectionErrorHandler(
      const Closure& handler) = 0;

  // Returns the id of the remote application. For Connections created via
  // Shell::Connect(), this will not be determined until Connect()'s callback is
  // run, and this function will return false. Use AddRemoteIDCallback() to
  // schedule a callback to be run when the remote application id is available.
  // A value of Shell::kInvalidApplicationID indicates the connection has not
  // been established.
  virtual bool GetRemoteApplicationID(uint32_t* remote_id) const = 0;

  // Returns the id of the deepest content handler used in connecting to
  // the application. See GetRemoteApplicationID() for details about the return
  // value. A |content_handler_id| value of Shell::kInvalidApplicationID
  // indicates no content handler was used in connecting to the application.
  virtual bool GetRemoteContentHandlerID(
      uint32_t* content_handler_id) const = 0;

  // See description in GetRemoteApplicationID()/GetRemoteContentHandlerID(). If
  // the ids are available, |callback| is run immediately.
  virtual void AddRemoteIDCallback(const Closure& callback) = 0;

 protected:
  // Returns true if the binder was set, false if it was not set (e.g. by
  // some filtering policy preventing this interface from being exposed).
   virtual bool SetInterfaceBinderForName(InterfaceBinder* binder,
                                          const std::string& name) = 0;

  virtual base::WeakPtr<Connection> GetWeakPtr() = 0;
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_CONNECTION_H_
