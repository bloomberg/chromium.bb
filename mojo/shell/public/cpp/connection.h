// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_CONNECTION_H_
#define MOJO_SHELL_PUBLIC_CPP_CONNECTION_H_

#include <stdint.h>

#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "mojo/shell/public/cpp/connect.h"
#include "mojo/shell/public/cpp/interface_registry.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"

namespace mojo {

class InterfaceBinder;

// Represents a connection to another application. An instance of this class is
// returned from Shell's ConnectToApplication(), and passed to ShellClient's
// AcceptConnection() each time an incoming connection is received.
//
// Call AddService<T>(factory) to expose an interface to the remote application,
// and GetInterface(&interface_ptr) to consume an interface exposed by the
// remote application.
//
// Internally, this class wraps an InterfaceRegistry that accepts interfaces
// that may be exposed to a remote application. See documentation in
// interface_registry.h for more information.
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

  // Allow the remote application to request instances of Interface.
  // |factory| will create implementations of Interface on demand.
  // Returns true if the interface was exposed, false if capability filtering
  // from the shell prevented the interface from being exposed.
  template <typename Interface>
  bool AddInterface(InterfaceFactory<Interface>* factory) {
    return GetLocalRegistry()->AddInterface<Interface>(factory);
  }

  // Binds |ptr| to an implemention of Interface in the remote application.
  // |ptr| can immediately be used to start sending requests to the remote
  // interface.
  template <typename Interface>
  void GetInterface(InterfacePtr<Interface>* ptr) {
    mojo::GetInterface(GetRemoteInterfaces(), ptr);
  }

  // Returns the name that was used by the source application to establish a
  // connection to the destination application.
  //
  // When Connection is representing and outgoing connection, this will be the
  // same as the value returned by GetRemoveApplicationName().
  virtual const std::string& GetConnectionName() = 0;

  // Returns the name identifying the remote application on this connection.
  virtual const std::string& GetRemoteApplicationName() = 0;

  // Returns the User ID for the remote application. Prior to the Connect()
  // callback being fired, this will return the value passed via Connect().
  // After the Connect() callback (call AddRemoteIDCallback to register one)
  // this will return the actual user id the shell ran the target as.
  virtual const std::string& GetRemoteUserID() const = 0;

  // Register a handler to receive an error notification on the pipe to the
  // remote application's InterfaceProvider.
  virtual void SetConnectionLostClosure(const Closure& handler) = 0;

  // Returns true if |result| has been modified to include the result of the
  // connection (see mojo/shell/public/interfaces/connector.mojom for error
  // codes), false if the connection is still pending and |result| has not been
  // modified. Call AddConnectionCompletedClosure() to schedule a closure to be
  // run when the connection is completed and the result is available.
  virtual bool GetConnectionResult(
      shell::mojom::ConnectResult* result) const = 0;

  // Returns true if |remote_id| has been modified to include the instance id of
  // the remote application. For connections created via Connector::Connect(),
  // this will not be determined until the connection has been completed by the
  // shell. Use AddConnectionCompletedClosure() to schedule a closure to be run
  // when the connection is completed and the remote id is available. Returns
  // false if the connection has not yet been completed and |remote_id| is not
  // modified.
  virtual bool GetRemoteApplicationID(uint32_t* remote_id) const = 0;

  // Register a closure to be run when the connection has been completed by the
  // shell and remote metadata is available. Useful only for connections created
  // via Connector::Connect(). Once the connection is complete, metadata is
  // available immediately.
  virtual void AddConnectionCompletedClosure(const Closure& callback) = 0;

  // Returns true if the Shell allows |interface_name| to be exposed to the
  // remote application.
  virtual bool AllowsInterface(const std::string& interface_name) const = 0;

  // Returns the raw proxy to the remote application's InterfaceProvider
  // interface. Most applications will just use GetInterface() instead.
  // Caller does not take ownership.
  virtual shell::mojom::InterfaceProvider* GetRemoteInterfaces() = 0;

 protected:
  virtual InterfaceRegistry* GetLocalRegistry() = 0;

  virtual base::WeakPtr<Connection> GetWeakPtr() = 0;
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_CONNECTION_H_
