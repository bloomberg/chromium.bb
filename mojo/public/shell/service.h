// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SHELL_SERVICE_H_
#define MOJO_PUBLIC_SHELL_SERVICE_H_

#include <vector>

#include "mojo/public/bindings/error_handler.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/system/core_cpp.h"
#include "mojom/shell.h"

// Utility classes for creating ShellClients that vend service instances.
// To use define a class that implements your specific server api, e.g. FooImpl
// to implement a service named Foo. That class must define an empty constructor
// and the Initialize() method.
// class FooImpl : public Foo {
//  public:
//   FooImpl();
//   void Initialize(ServiceFactory<FooImpl>* service_factory,
//                   ScopedMessagePipeHandle client_handle
//  private:
//   ServiceFactory<FooImpl>* service_factory_;
//   RemotePtr<FooPeer> client_;
// };
//
//
// To simplify further FooImpl can use the Service<> template.
// class FooImpl : public Service<Foo, FooImpl> {
//  public:
//   FooImpl();
//   ...
//   <Foo implementation>
// };
//
// Instances of FooImpl will be created by a specialized ServiceFactory
//
// ServiceFactory<FooImpl>
//
// Optionally the classes can be specializeed with a shared context
// class ServiceFactory<FooImpl, MyContext>
// and
// class FooImpl : public Service<Foo, FooImpl, MyContext>
//
// foo_factory = new ServiceFactory<FooImpl, MyContext>(my_context);
// instances of FooImpl can call context() and retrieve the value of my_context.
//
// Lastly create an Application instance that collects all the ServiceFactories.
//
// Application app(shell_handle);
// app.AddServiceFactory(new ServiceFactory<FooImpl>);
//
//
// Specialization of ServiceFactory.
// ServiceImpl: Implementation of Service interface.
// Context: Optional type of shared context.v
//
//
namespace mojo {

namespace internal {
class ServiceFactoryBase {
 public:
  class Owner {
   public:
    virtual Shell* GetShell() = 0;
    virtual void AddServiceFactory(
        internal::ServiceFactoryBase* service_factory) = 0;
    virtual void RemoveServiceFactory(
        internal::ServiceFactoryBase* service_factory) = 0;

   protected:
    void set_service_factory_owner(ServiceFactoryBase* service_factory,
                                   Owner* owner) {
      service_factory->owner_ = owner;
    }
  };
  ServiceFactoryBase() : owner_(NULL) {}
  virtual ~ServiceFactoryBase();
  Shell* GetShell() { return owner_->GetShell(); }
  virtual void AcceptConnection(const std::string& url,
                                ScopedMessagePipeHandle client_handle) = 0;

 protected:
  Owner* owner_;
};
}  // namespace internal

template <class ServiceImpl, typename Context=void>
class ServiceFactory : public internal::ServiceFactoryBase {
 public:
  ServiceFactory(Context* context = NULL) : context_(context) {}

  virtual ~ServiceFactory() {
    for (typename ServiceList::iterator it = services_.begin();
         it != services_.end(); ++it) {
      delete *it;
    }
  }

  virtual void AcceptConnection(const std::string& url,
                                ScopedMessagePipeHandle client_handle)
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
        delete service;
        if (services_.empty())
          owner_->RemoveServiceFactory(this);
        return;
      }
    }
  }

  Context* context() const { return context_; }

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
  virtual ~Service() {}

 protected:
  Service() : reaper_(this), service_factory_(NULL) {}

  void Initialize(ServiceFactory<ServiceImpl, Context>* service_factory,
                  ScopedMessagePipeHandle client_handle) {
    service_factory_ = service_factory;
    client_.reset(
        MakeScopedHandle(
            InterfaceHandle<typename ServiceInterface::_Peer>(
                client_handle.release().value())).Pass(),
        this,
        &reaper_);
  }

  Context* context() const { return service_factory_->context(); }
  typename ServiceInterface::_Peer* client() { return client_.get(); }

 private:
  // The Reaper class allows us to handle errors on the client proxy without
  // polluting the name space of the Service<> class.
  class Reaper : public ErrorHandler {
   public:
    Reaper(Service<ServiceInterface, ServiceImpl, Context>* service)
        : service_(service) {}
    virtual void OnError() {
      service_->service_factory_->RemoveService(
          static_cast<ServiceImpl*>(service_));
    }
   private:
    Service<ServiceInterface, ServiceImpl, Context>* service_;
  };
  friend class ServiceFactory<ServiceImpl, Context>;
  Reaper reaper_;
  ServiceFactory<ServiceImpl, Context>* service_factory_;
  RemotePtr<typename ServiceInterface::_Peer> client_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SHELL_SERVICE_H_
