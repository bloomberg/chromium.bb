// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_SHELL_H_
#define MOJO_SHELL_PUBLIC_CPP_SHELL_H_

#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "url/gurl.h"

namespace mojo {

// An interface implementation can keep this object as a member variable to
// hold a reference to the ShellConnection, keeping it alive as long as the
// bound implementation exists.
// Since interface implementations can be bound on different threads than the
// ShellConnection, this class is safe to use on any thread. However, each
// instance should only be used on one thread at a time (otherwise there'll be
// races between the AddRef resulting from cloning and destruction).
class AppRefCount {
 public:
  virtual ~AppRefCount() {}

  virtual scoped_ptr<AppRefCount> Clone() = 0;
};

// An interface that encapsulates the Mojo Shell's broker interface by which
// connections between applications are established. Implemented by
// ShellConnection, this is the primary interface exposed to clients.
class Shell {
 public:
  // Requests a new connection to an application. Returns a pointer to the
  // connection if the connection is permitted by this application's delegate,
  // or nullptr otherwise. Caller takes ownership.
  virtual scoped_ptr<Connection> Connect(const std::string& url) = 0;
  virtual scoped_ptr<Connection> Connect(Connector::ConnectParams* params) = 0;

  // Connect to application identified by |request->url| and connect to the
  // service implementation of the interface identified by |Interface|.
  template <typename Interface>
  void ConnectToInterface(Connector::ConnectParams* params,
                          InterfacePtr<Interface>* ptr) {
    scoped_ptr<Connection> connection = Connect(params);
    if (connection)
      connection->GetInterface(ptr);
  }
  template <typename Interface>
  void ConnectToInterface(const std::string& url,
                          InterfacePtr<Interface>* ptr) {
    Connector::ConnectParams params(url);
    return ConnectToInterface(&params, ptr);
  }

  // Returns a clone of the ShellConnection's Connector that can be passed to
  // other threads.
  virtual scoped_ptr<Connector> CloneConnector() const = 0;

  // Initiate shutdown of this application. This may involve a round trip to the
  // Shell to ensure there are no inbound service requests.
  virtual void Quit() = 0;

  // Create an object that can be used to refcount the lifetime of the
  // application. The returned object may be cloned, and when the refcount falls
  // to zero Quit() is called.
  virtual scoped_ptr<AppRefCount> CreateAppRefCount() = 0;
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_SHELL_H_
