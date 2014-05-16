// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SHELL_SERVICE_H_
#define MOJO_PUBLIC_SHELL_SERVICE_H_

#include <assert.h>

#include <vector>

#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"

// Utility classes for creating ShellClients that vend service instances.
// To use define a class that implements your specific server api, e.g. FooImpl
// to implement a service named Foo. That class must define an empty constructor
// and the Initialize() method.
// class FooImpl : public Foo {
//  public:
//   FooImpl();
//   void Initialize();
//  private:
//   ServiceConnector<FooImpl>* service_connector_;
// };
//
//
// To simplify further FooImpl can use the ServiceConnection<> template.
// class FooImpl : public ServiceConnection<Foo, FooImpl> {
//  public:
//   FooImpl();
//   ...
//   <Foo implementation>
// };
//
// Instances of FooImpl will be created by a specialized ServiceConnector
//
// ServiceConnector<FooImpl>
//
// Optionally the classes can be specializeed with a shared context
// class ServiceConnector<FooImpl, MyContext>
// and
// class FooImpl : public ServiceConnection<Foo, FooImpl, MyContext>
//
// foo_connector = new ServiceConnector<FooImpl, MyContext>(my_context);
// instances of FooImpl can call context() and retrieve the value of my_context.
//
// Lastly create an Application instance that collects all the
// ServiceConnectors.
//
// Application app(shell_handle);
// app.AddServiceConnector(new ServiceConnector<FooImpl>);
//
//
// Specialization of ServiceConnector.
// ServiceImpl: Implementation of Service interface.
// Context: Optional type of shared context.v
//
//
namespace mojo {

namespace internal {
class ServiceConnectorBase {
 public:
  class Owner : public ShellClient {
   public:
    Owner(ScopedMessagePipeHandle shell_handle);
    virtual ~Owner();
    Shell* shell() { return shell_.get(); }
    virtual void AddServiceConnector(
        internal::ServiceConnectorBase* service_connector) = 0;
    virtual void RemoveServiceConnector(
        internal::ServiceConnectorBase* service_connector) = 0;

   protected:
    void set_service_connector_owner(ServiceConnectorBase* service_connector,
                                     Owner* owner) {
      service_connector->owner_ = owner;
    }
    ShellPtr shell_;
  };
  ServiceConnectorBase() : owner_(NULL) {}
  virtual ~ServiceConnectorBase();
  Shell* shell() { return owner_->shell(); }
  virtual void AcceptConnection(const std::string& url,
                                ScopedMessagePipeHandle client_handle) = 0;

 protected:
  Owner* owner_;
};
}  // namespace internal

template <class ServiceImpl, typename Context=void>
class ServiceConnector : public internal::ServiceConnectorBase {
 public:
  ServiceConnector(Context* context = NULL) : context_(context) {}

  virtual ~ServiceConnector() {
    ConnectionList doomed;
    doomed.swap(connections_);
    for (typename ConnectionList::iterator it = doomed.begin();
         it != doomed.end(); ++it) {
      delete *it;
    }
    assert(connections_.empty());  // No one should have added more!
  }

  virtual void AcceptConnection(const std::string& url,
                                ScopedMessagePipeHandle handle) MOJO_OVERRIDE {
    ServiceImpl* impl = BindToPipe(new ServiceImpl(), handle.Pass());
    impl->set_connector(this);

    connections_.push_back(impl);

    impl->Initialize();
  }

  void RemoveConnection(ServiceImpl* impl) {
    // Called from ~ServiceImpl, in response to a connection error.
    for (typename ConnectionList::iterator it = connections_.begin();
         it != connections_.end(); ++it) {
      if (*it == impl) {
        delete impl;
        connections_.erase(it);
        if (connections_.empty())
          owner_->RemoveServiceConnector(this);
        return;
      }
    }
  }

  Context* context() const { return context_; }

 private:
  typedef std::vector<ServiceImpl*> ConnectionList;
  ConnectionList connections_;
  Context* context_;
};

// Specialization of ServiceConnection.
// ServiceInterface: Service interface.
// ServiceImpl: Subclass of ServiceConnection<...>.
// Context: Optional type of shared context.
template <class ServiceInterface, class ServiceImpl, typename Context=void>
class ServiceConnection : public InterfaceImpl<ServiceInterface> {
 protected:
  // NOTE: shell() and context() are not available at construction time.
  // Initialize() will be called once those are available.
  ServiceConnection() : service_connector_(NULL) {}

  virtual ~ServiceConnection() {}

  virtual void OnConnectionError() MOJO_OVERRIDE {
    service_connector_->RemoveConnection(static_cast<ServiceImpl*>(this));
  }

  // Shadow this method in ServiceImpl to perform one-time initialization.
  // At the time this is called, shell() and context() will be available.
  // NOTE: No need to call the base class Initialize from your subclass. It
  // will always be a no-op.
  void Initialize() {}

  Shell* shell() {
    return service_connector_->shell();
  }

  Context* context() const {
    return service_connector_->context();
  }

 private:
  friend class ServiceConnector<ServiceImpl, Context>;

  // Called shortly after this class is instantiated.
  void set_connector(ServiceConnector<ServiceImpl, Context>* connector) {
    service_connector_ = connector;
  }

  ServiceConnector<ServiceImpl, Context>* service_connector_;
};

template <typename Interface>
inline void ConnectTo(Shell* shell, const std::string& url,
                      InterfacePtr<Interface>* ptr) {
  MessagePipe pipe;
  ptr->Bind(pipe.handle0.Pass());

  AllocationScope scope;
  shell->Connect(url, pipe.handle1.Pass());
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_SHELL_SERVICE_H_
