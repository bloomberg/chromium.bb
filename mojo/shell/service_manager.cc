// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/service_manager.h"

#include "base/logging.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/error_handler.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojom/shell.h"

namespace mojo {
namespace shell {

class ServiceManager::ServiceFactory : public Shell, public ErrorHandler {
 public:
  ServiceFactory(ServiceManager* manager, const GURL& url)
      : manager_(manager),
        url_(url) {
    InterfacePipe<Shell> pipe;
    shell_client_.reset(pipe.handle_to_peer.Pass(), this, this);
    manager_->GetLoaderForURL(url)->Load(url, pipe.handle_to_self.Pass());
  }
  virtual ~ServiceFactory() {}

  void ConnectToClient(ScopedMessagePipeHandle handle) {
    if (handle.is_valid()) {
      AllocationScope scope;
      shell_client_->AcceptConnection(url_.spec(), handle.Pass());
    }
  }

  virtual void Connect(const String& url,
                       ScopedMessagePipeHandle client_pipe) MOJO_OVERRIDE {
    manager_->Connect(GURL(url.To<std::string>()), client_pipe.Pass());
  }

  virtual void OnError() MOJO_OVERRIDE {
    manager_->RemoveServiceFactory(this);
  }

  const GURL& url() const { return url_; }

 private:
  ServiceManager* const manager_;
  const GURL url_;
  RemotePtr<ShellClient> shell_client_;
  DISALLOW_COPY_AND_ASSIGN(ServiceFactory);
};

ServiceManager::Loader::Loader() {}
ServiceManager::Loader::~Loader() {}

bool ServiceManager::TestAPI::HasFactoryForURL(const GURL& url) const {
  return manager_->url_to_service_factory_.find(url) !=
      manager_->url_to_service_factory_.end();
}

ServiceManager::ServiceManager() : default_loader_(NULL) {
}

ServiceManager::~ServiceManager() {
  for (ServiceFactoryMap::iterator it = url_to_service_factory_.begin();
       it != url_to_service_factory_.end(); ++it) {
    delete it->second;
  }
  url_to_service_factory_.clear();
}

void ServiceManager::SetLoaderForURL(Loader* loader, const GURL& gurl) {
  DCHECK(url_to_loader_.find(gurl) == url_to_loader_.end());
  url_to_loader_[gurl] = loader;
}

ServiceManager::Loader* ServiceManager::GetLoaderForURL(const GURL& gurl) {
  LoaderMap::const_iterator it = url_to_loader_.find(gurl);
  if (it != url_to_loader_.end())
    return it->second;
  DCHECK(default_loader_);
  return default_loader_;
}

void ServiceManager::Connect(const GURL& url,
                               ScopedMessagePipeHandle client_handle) {
  ServiceFactoryMap::const_iterator service_it =
      url_to_service_factory_.find(url);
  ServiceFactory* service_factory;
  if (service_it != url_to_service_factory_.end()) {
    service_factory = service_it->second;
  } else {
    service_factory = new ServiceFactory(this, url);
    url_to_service_factory_[url] = service_factory;
  }
  service_factory->ConnectToClient(client_handle.Pass());
}

void ServiceManager::RemoveServiceFactory(ServiceFactory* service_factory) {
  ServiceFactoryMap::iterator it =
      url_to_service_factory_.find(service_factory->url());
  DCHECK(it != url_to_service_factory_.end());
  delete it->second;
  url_to_service_factory_.erase(it);
}

}  // namespace shell
}  // namespace mojo
