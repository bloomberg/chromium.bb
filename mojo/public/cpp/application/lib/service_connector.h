// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_H_
#define MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_H_

#include <assert.h>

#include <vector>

#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"

namespace mojo {
namespace internal {

template <class ServiceImpl, typename Context>
class ServiceConnector;

// Specialization of ServiceConnection.
// ServiceImpl: Subclass of InterfaceImpl<...>.
// Context: Type of shared context.
template <class ServiceImpl, typename Context>
class ServiceConnection : public ServiceImpl {
 public:
  ServiceConnection() : ServiceImpl() {}
  ServiceConnection(Context* context) : ServiceImpl(context) {}

  virtual void OnConnectionError() MOJO_OVERRIDE {
    service_connector_->RemoveConnection(static_cast<ServiceImpl*>(this));
    ServiceImpl::OnConnectionError();
  }

private:
  friend class ServiceConnector<ServiceImpl, Context>;

  // Called shortly after this class is instantiated.
  void set_service_connector(
      ServiceConnector<ServiceImpl, Context>* connector) {
    service_connector_ = connector;
  }

  ServiceConnector<ServiceImpl, Context>* service_connector_;
};

template <typename ServiceImpl, typename Context>
struct ServiceConstructor {
  static ServiceConnection<ServiceImpl, Context>* New(Context* context) {
    return new ServiceConnection<ServiceImpl, Context>(context);
  }
};

template <typename ServiceImpl>
struct ServiceConstructor<ServiceImpl, void> {
 public:
  static ServiceConnection<ServiceImpl, void>* New(void* context) {
    return new ServiceConnection<ServiceImpl, void>();
  }
};

class ServiceConnectorBase {
 public:
  class Owner : public ServiceProvider {
   public:
    Owner();
    Owner(ScopedMessagePipeHandle service_provider_handle);
    virtual ~Owner();
    virtual void AddServiceConnector(
        internal::ServiceConnectorBase* service_connector) = 0;
    virtual void RemoveServiceConnector(
        internal::ServiceConnectorBase* service_connector) = 0;

   protected:
    void set_service_connector_owner(ServiceConnectorBase* service_connector,
                                     Owner* owner) {
      service_connector->owner_ = owner;
    }
    ServiceProviderPtr service_provider_;
  };
  ServiceConnectorBase() : owner_(NULL) {}
  virtual ~ServiceConnectorBase();
  virtual void ConnectToService(const std::string& url,
                                ScopedMessagePipeHandle client_handle) = 0;

 protected:
  Owner* owner_;
};

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

  virtual void ConnectToService(const std::string& url,
                                ScopedMessagePipeHandle handle) MOJO_OVERRIDE {
    ServiceConnection<ServiceImpl, Context>* impl =
        ServiceConstructor<ServiceImpl, Context>::New(context_);
    impl->set_service_connector(this);
    BindToPipe(impl, handle.Pass());

    connections_.push_back(impl);
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

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_H_
