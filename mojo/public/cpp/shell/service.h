// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SHELL_SERVICE_H_
#define MOJO_PUBLIC_SHELL_SERVICE_H_

#include <vector>

#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"

// Utility classes for creating ShellClients that vend service instances.
// To use define a class that implements your specific server api, e.g. FooImpl
// to implement a service named Foo. That class must define an empty constructor
// and the Initialize() method.
// class FooImpl : public Foo {
//  public:
//   FooImpl();
//   void Initialize(ServiceConnector<FooImpl>* service_connector,
//                   ScopedMessagePipeHandle client_handle
//  private:
//   ServiceConnector<FooImpl>* service_connector_;
//   RemotePtr<FooPeer> client_;
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
    Owner(ScopedShellHandle shell_handle);
    ~Owner();
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
    RemotePtr<Shell> shell_;
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
          owner_->RemoveServiceConnector(this);
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

// Specialization of ServiceConnection.
// ServiceInterface: Service interface.
// ServiceImpl: Implementation of Service interface.
// Context: Optional type of shared context.
template <class ServiceInterface, class ServiceImpl, typename Context=void>
class ServiceConnection : public ServiceInterface {
 public:
  virtual ~ServiceConnection() {}

 protected:
  ServiceConnection() : reaper_(this), service_connector_(NULL) {}

  void Initialize(ServiceConnector<ServiceImpl, Context>* service_connector,
                  ScopedMessagePipeHandle client_handle) {
    service_connector_ = service_connector;
    client_.reset(
        MakeScopedHandle(
            InterfaceHandle<typename ServiceInterface::_Peer>(
                client_handle.release().value())).Pass(),
        this,
        &reaper_);
  }

  Shell* shell() { return service_connector_->shell(); }
  Context* context() const { return service_connector_->context(); }
  typename ServiceInterface::_Peer* client() { return client_.get(); }

 private:
  // The Reaper class allows us to handle errors on the client proxy without
  // polluting the name space of the ServiceConnection<> class.
  class Reaper : public ErrorHandler {
   public:
    Reaper(ServiceConnection<ServiceInterface, ServiceImpl, Context>* service)
        : service_(service) {}
    virtual void OnError() {
      service_->service_connector_->RemoveService(
          static_cast<ServiceImpl*>(service_));
    }
   private:
    ServiceConnection<ServiceInterface, ServiceImpl, Context>* service_;
  };
  friend class ServiceConnector<ServiceImpl, Context>;
  Reaper reaper_;
  ServiceConnector<ServiceImpl, Context>* service_connector_;
  RemotePtr<typename ServiceInterface::_Peer> client_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SHELL_SERVICE_H_
