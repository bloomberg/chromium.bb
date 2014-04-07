// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "mojo/service_manager/service_manager.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/service_manager/service_loader.h"

namespace mojo {

namespace {
// Used by TestAPI.
bool has_created_instance = false;
}

class ServiceManager::ServiceFactory : public Shell, public ErrorHandler {
 public:
  ServiceFactory(ServiceManager* manager, const GURL& url)
      : manager_(manager),
        url_(url) {
    InterfacePipe<Shell> pipe;
    shell_client_.reset(pipe.handle_to_peer.Pass(), this, this);
    manager_->GetLoaderForURL(url)->LoadService(manager_,
                                                url,
                                                pipe.handle_to_self.Pass());
  }

  virtual ~ServiceFactory() {}

  void ConnectToClient(ScopedMessagePipeHandle handle) {
    if (handle.is_valid()) {
      AllocationScope scope;
      shell_client_->AcceptConnection(url_.spec(), handle.Pass());
    }
  }

  virtual void Connect(const String& url,
                       ScopedMessagePipeHandle client_pipe) OVERRIDE {
    manager_->Connect(GURL(url.To<std::string>()), client_pipe.Pass());
  }

  virtual void OnError() OVERRIDE {
    manager_->OnServiceFactoryError(this);
  }

  const GURL& url() const { return url_; }

 private:
  ServiceManager* const manager_;
  const GURL url_;
  RemotePtr<ShellClient> shell_client_;
  DISALLOW_COPY_AND_ASSIGN(ServiceFactory);
};

// static
bool ServiceManager::TestAPI::HasCreatedInstance() {
  return has_created_instance;
}

bool ServiceManager::TestAPI::HasFactoryForURL(const GURL& url) const {
  return manager_->url_to_service_factory_.find(url) !=
      manager_->url_to_service_factory_.end();
}

ServiceManager::ServiceManager()
    : default_loader_(NULL),
      interceptor_(NULL) {
}

ServiceManager::~ServiceManager() {
  for (URLToServiceFactoryMap::iterator it = url_to_service_factory_.begin();
       it != url_to_service_factory_.end(); ++it) {
    delete it->second;
  }
  url_to_service_factory_.clear();
}

// static
ServiceManager* ServiceManager::GetInstance() {
  static base::LazyInstance<ServiceManager> instance =
      LAZY_INSTANCE_INITIALIZER;
  has_created_instance = true;
  return &instance.Get();
}

void ServiceManager::Connect(const GURL& url,
                             ScopedMessagePipeHandle client_handle) {
  URLToServiceFactoryMap::const_iterator service_it =
      url_to_service_factory_.find(url);
  ServiceFactory* service_factory;
  if (service_it != url_to_service_factory_.end()) {
    service_factory = service_it->second;
  } else {
    service_factory = new ServiceFactory(this, url);
    url_to_service_factory_[url] = service_factory;
  }
  if (interceptor_) {
    service_factory->ConnectToClient(
        interceptor_->OnConnectToClient(url, client_handle.Pass()));
  } else {
    service_factory->ConnectToClient(client_handle.Pass());
  }
}

void ServiceManager::SetLoaderForURL(ServiceLoader* loader, const GURL& url) {
  DCHECK(url_to_loader_.find(url) == url_to_loader_.end());
  url_to_loader_[url] = loader;
}

void ServiceManager::SetLoaderForScheme(ServiceLoader* loader,
                                        const std::string& scheme) {
  DCHECK(scheme_to_loader_.find(scheme) == scheme_to_loader_.end());
  scheme_to_loader_[scheme] = loader;
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
  return default_loader_;
}

void ServiceManager::OnServiceFactoryError(ServiceFactory* service_factory) {
  const GURL url = service_factory->url();
  URLToServiceFactoryMap::iterator it = url_to_service_factory_.find(url);
  DCHECK(it != url_to_service_factory_.end());
  delete it->second;
  url_to_service_factory_.erase(it);
  GetLoaderForURL(url)->OnServiceError(this, url);
}

}  // namespace mojo
