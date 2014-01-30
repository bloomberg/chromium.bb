// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SHELL_SERVICE_H_
#define MOJO_PUBLIC_SHELL_SERVICE_H_

#include <vector>

#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/system/core_cpp.h"
#include "mojom/shell.h"

// Utility classes for creating ShellClients that vend service instances.
// To use define a class that implements your specific server api, e.g. FooImpl
// to implement a service named Foo. That class must define an empty constructor
// and the Initialize() method.
// class FooImpl : public Foo {
//  public:
//   typedef MyContext SharedContext;
//   FooImpl();
//   void Initialize(ServiceFactory<FooImpl>* service_factory,
//                   ScopedMessagePipeHandle client_handle
//  private:
//   ServiceFactory<FooImpl>* service_factory_;
//   RemotePtr<FooPeer> client_;
// };
//
// To instantiate such a service:
// ServiceFactory<FooImpl> service_factory(shell_handle.Pass());
//
// To simplify further Foo can use the Service<> template.
// class FooImpl : public Service<Foo, FooImpl> {
//  public:
//   FooImpl();
//   ...
//   <Foo implementation>
// };
//
// Optionally the clases can be specializeed with a shared context
// class ServiceFactory<FooImpl, MyContext>
// and
// class FooImpl : public Service<Foo, FooImpl, MyContext>
//
// ServiceFactory<FooImpl> service_factory(shell_handle.Pass(), my_context)
// instances of FooImpl can call context() and retrieve the value of my_context.
//
// Specialization of ServiceFactory.
// ServiceImpl: Implementation of Service interface.
// Context: Optional type of shared context.v
namespace mojo {

class ServiceFactoryBase : public ShellClient {
 public:
  virtual ~ServiceFactoryBase();

 protected:
  ServiceFactoryBase(ScopedMessagePipeHandle shell_handle);
  Shell* shell() { return shell_.get(); }

 private:
  RemotePtr<Shell> shell_;
};

template <class ServiceImpl, typename Context=void>
class ServiceFactory : public ServiceFactoryBase {
 public:
  ServiceFactory(ScopedMessagePipeHandle shell_handle, Context* context = NULL)
      : ServiceFactoryBase(shell_handle.Pass()),
        context_(context) {
  }

  virtual ~ServiceFactory() {
    for (typename ServiceList::iterator it = services_.begin();
         it != services_.end(); ++it) {
      delete *it;
    }
  }

  virtual void AcceptConnection(ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE {
    ServiceImpl* service = new ServiceImpl();
    service->Initialize(this, client_handle.Pass());
    services_.push_back(service);
  }

  void RemoveService(ServiceImpl* service) {
    for (typename ServiceList::iterator it = services_.begin();
         it != services_.end(); ++it) {
      if (*it == service) {
        services_.erase(it);
        return;
      }
    }
  }

  Context* context() const {
    return context_;
  }

 private:
  typedef std::vector<ServiceImpl*> ServiceList;
  ServiceList services_;
  Context* context_;
};

// Specialization of ServiceFactory.
// ServiceInterface: Service interface.
// ServiceImpl: Implementation of Service interface.
// Context: Optional type of shared context.
template <class ServiceInterface, class ServiceImpl, typename Context=void>
class Service : public ServiceInterface {
 public:
  virtual ~Service() {
    service_factory_->RemoveService(static_cast<ServiceImpl*>(this));
  }

 protected:
  friend class ServiceFactory<ServiceImpl, Context>;
  Service() {}

  void Initialize(ServiceFactory<ServiceImpl, Context>* service_factory,
                  ScopedMessagePipeHandle client_handle) {
    service_factory_ = service_factory;
    client_.reset(client_handle.Pass(), this);
  }

  Context* context() const {
    return service_factory_->context();
  }

  typename ServiceInterface::_Peer* client() { return client_.get(); }

 private:
  ServiceFactory<ServiceImpl, Context>* service_factory_;
  RemotePtr<typename ServiceInterface::_Peer> client_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SHELL_SERVICE_H_
