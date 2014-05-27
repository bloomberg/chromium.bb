// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "mojo/service_manager/service_manager.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/service_manager/service_loader.h"

namespace mojo {

namespace {
// Used by TestAPI.
bool has_created_instance = false;
}

class ServiceManager::ServiceFactory : public InterfaceImpl<ServiceProvider> {
 public:
  ServiceFactory(ServiceManager* manager, const GURL& url)
      : manager_(manager),
        url_(url) {
  }

  virtual ~ServiceFactory() {
  }

  void ConnectToClient(ScopedMessagePipeHandle handle) {
    if (handle.is_valid()) {
      AllocationScope scope;
      client()->ConnectToService(url_.spec(), handle.Pass());
    }
  }

  // ServiceProvider implementation:
  virtual void ConnectToService(const String& url,
                                ScopedMessagePipeHandle client_pipe) OVERRIDE {
    manager_->ConnectToService(GURL(url.To<std::string>()), client_pipe.Pass());
  }

  const GURL& url() const { return url_; }

 private:
  virtual void OnConnectionError() OVERRIDE {
    manager_->OnServiceFactoryError(this);
  }

  ServiceManager* const manager_;
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ServiceFactory);
};

class ServiceManager::TestAPI::TestServiceProviderConnection
    : public InterfaceImpl<ServiceProvider> {
 public:
  explicit TestServiceProviderConnection(ServiceManager* manager)
      : manager_(manager) {}
  virtual ~TestServiceProviderConnection() {}

  virtual void OnConnectionError() OVERRIDE {
    // TODO(darin): How should we handle this error?
  }

  // ServiceProvider:
  virtual void ConnectToService(const String& url,
                                ScopedMessagePipeHandle client_pipe) OVERRIDE {
    manager_->ConnectToService(GURL(url.To<std::string>()), client_pipe.Pass());
  }

 private:
  ServiceManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceProviderConnection);
};

// static
ServiceManager::TestAPI::TestAPI(ServiceManager* manager) : manager_(manager) {
}

ServiceManager::TestAPI::~TestAPI() {
}

bool ServiceManager::TestAPI::HasCreatedInstance() {
  return has_created_instance;
}

ScopedMessagePipeHandle ServiceManager::TestAPI::GetServiceProviderHandle() {
  MessagePipe pipe;
  service_provider_.reset(
      BindToPipe(new TestServiceProviderConnection(manager_),
                 pipe.handle0.Pass()));
  return pipe.handle1.Pass();
}

bool ServiceManager::TestAPI::HasFactoryForURL(const GURL& url) const {
  return manager_->url_to_service_factory_.find(url) !=
         manager_->url_to_service_factory_.end();
}

ServiceManager::ServiceManager()
    : interceptor_(NULL) {
}

ServiceManager::~ServiceManager() {
  STLDeleteValues(&url_to_service_factory_);
  STLDeleteValues(&url_to_loader_);
  STLDeleteValues(&scheme_to_loader_);
}

// static
ServiceManager* ServiceManager::GetInstance() {
  static base::LazyInstance<ServiceManager> instance =
      LAZY_INSTANCE_INITIALIZER;
  has_created_instance = true;
  return &instance.Get();
}

void ServiceManager::ConnectToService(const GURL& url,
                                      ScopedMessagePipeHandle client_handle) {
  URLToServiceFactoryMap::const_iterator service_it =
      url_to_service_factory_.find(url);
  ServiceFactory* service_factory;
  if (service_it != url_to_service_factory_.end()) {
    service_factory = service_it->second;
  } else {
    MessagePipe pipe;
    GetLoaderForURL(url)->LoadService(this, url, pipe.handle0.Pass());

    service_factory =
        BindToPipe(new ServiceFactory(this, url), pipe.handle1.Pass());

    url_to_service_factory_[url] = service_factory;
  }
  if (interceptor_) {
    service_factory->ConnectToClient(
        interceptor_->OnConnectToClient(url, client_handle.Pass()));
  } else {
    service_factory->ConnectToClient(client_handle.Pass());
  }
}

void ServiceManager::SetLoaderForURL(scoped_ptr<ServiceLoader> loader,
                                     const GURL& url) {
  URLToLoaderMap::iterator it = url_to_loader_.find(url);
  if (it != url_to_loader_.end())
    delete it->second;
  url_to_loader_[url] = loader.release();
}

void ServiceManager::SetLoaderForScheme(scoped_ptr<ServiceLoader> loader,
                                        const std::string& scheme) {
  SchemeToLoaderMap::iterator it = scheme_to_loader_.find(scheme);
  if (it != scheme_to_loader_.end())
    delete it->second;
  scheme_to_loader_[scheme] = loader.release();
}

void ServiceManager::SetInterceptor(Interceptor* interceptor) {
  interceptor_ = interceptor;
}

ServiceLoader* ServiceManager::GetLoaderForURL(const GURL& url) {
  URLToLoaderMap::const_iterator url_it = url_to_loader_.find(url);
  if (url_it != url_to_loader_.end())
    return url_it->second;
  SchemeToLoaderMap::const_iterator scheme_it =
      scheme_to_loader_.find(url.scheme());
  if (scheme_it != scheme_to_loader_.end())
    return scheme_it->second;
  DCHECK(default_loader_);
  return default_loader_.get();
}

void ServiceManager::OnServiceFactoryError(ServiceFactory* service_factory) {
  // Called from ~ServiceFactory, so we do not need to call Destroy here.
  const GURL url = service_factory->url();
  URLToServiceFactoryMap::iterator it = url_to_service_factory_.find(url);
  DCHECK(it != url_to_service_factory_.end());
  delete it->second;
  url_to_service_factory_.erase(it);
  GetLoaderForURL(url)->OnServiceError(this, url);
}

}  // namespace mojo
