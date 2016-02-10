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
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"

namespace mojo {

class InterfaceBinder;

// Represents a connection to another application. An instance of this class is
// returned from Shell's ConnectToApplication(), and passed to ShellClient's
// AcceptConnection() each time an incoming connection is received.
//
// To use, define a class that implements your specific interface. Then
// implement an InterfaceFactory<Foo> that binds instances of FooImpl to
// InterfaceRequest<Foo>s and register that on the connection like this:
//
//   connection->AddInterface(&factory);
//
// Or, if you have multiple factories implemented by the same type, explicitly
// specify the interface to register the factory for:
//
//   connection->AddInterface<Foo>(&my_foo_and_bar_factory_);
//   connection->AddInterface<Bar>(&my_foo_and_bar_factory_);
//
// The InterfaceFactory must outlive the Connection.
//
// Additionally you may specify a default InterfaceBinder to handle requests for
// interfaces unhandled by any registered InterfaceFactory. Just as with
// InterfaceFactory, the default InterfaceBinder supplied must outlive
// Connection.
//
// A Connection returned via Shell::ConnectToApplication() is owned by the
// caller.
// An Connection received via AcceptConnection is owned by the ShellConnection.
// To close a connection, call CloseConnection which will destroy this object.
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

  // Allow the remote application to request instances of Interface.
  // |factory| will create implementations of Interface on demand.
  // Returns true if the interface was exposed, false if capability filtering
  // from the shell prevented the interface from being exposed.
  template <typename Interface>
  bool AddInterface(InterfaceFactory<Interface>* factory) {
    return SetInterfaceBinderForName(
        new internal::InterfaceFactoryBinder<Interface>(factory),
        Interface::Name_);
  }

  // Binds |ptr| to an implemention of Interface in the remote application.
  // |ptr| can immediately be used to start sending requests to the remote
  // interface.
  template <typename Interface>
  void GetInterface(InterfacePtr<Interface>* ptr) {
    if (InterfaceProvider* ip = GetRemoteInterfaces()) {
      MessagePipe pipe;
      ptr->Bind(InterfacePtrInfo<Interface>(std::move(pipe.handle0), 0u));
      ip->GetInterface(Interface::Name_, std::move(pipe.handle1));
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

  // Returns the raw proxy to the remote application's InterfaceProvider
  // interface. Most applications will just use GetInterface() instead.
  // Caller does not take ownership.
  virtual InterfaceProvider* GetRemoteInterfaces() = 0;

  // Returns the local application's InterfaceProvider interface. The return
  // value is owned by this connection.
  virtual InterfaceProvider* GetLocalInterfaces() = 0;

  // Register a handler to receive an error notification on the pipe to the
  // remote application's InterfaceProvider.
  virtual void SetRemoteInterfaceProviderConnectionErrorHandler(
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
