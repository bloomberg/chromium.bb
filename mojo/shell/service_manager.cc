// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/service_manager.h"

#include "base/logging.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/shell.h"

namespace mojo {
namespace shell {

class ServiceManager::Service : public Shell {
 public:
  Service(ServiceManager* manager, const GURL& url)
      : manager_(manager),
        url_(url) {
    MessagePipe pipe;
    shell_client_.reset(pipe.handle0.Pass(), this);
    manager_->GetLoaderForURL(url)->Load(url, pipe.handle1.Pass());
  }
  virtual ~Service() {}

  void ConnectToClient(ScopedMessagePipeHandle handle) {
    if (handle.is_valid())
      shell_client_->AcceptConnection(handle.Pass());
  }

  virtual void Connect(const String& url,
                       ScopedMessagePipeHandle client_pipe) MOJO_OVERRIDE {
    manager_->Connect(GURL(url.To<std::string>()), client_pipe.Pass());
  }

 private:
  ServiceManager* manager_;
  GURL url_;
  RemotePtr<ShellClient> shell_client_;
  DISALLOW_COPY_AND_ASSIGN(Service);
};

ServiceManager::Loader::Loader() {}
ServiceManager::Loader::~Loader() {}

ServiceManager::ServiceManager() : default_loader_(NULL) {
}

ServiceManager::~ServiceManager() {
  for (ServiceMap::iterator it = url_to_service_.begin();
       it != url_to_service_.end(); ++it) {
    delete it->second;
  }
  url_to_service_.clear();
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
  ServiceMap::const_iterator service_it = url_to_service_.find(url);
  Service* service;
  if (service_it != url_to_service_.end()) {
    service = service_it->second;
  } else {
    service = new Service(this, url);
    url_to_service_[url] = service;
  }
  service->ConnectToClient(client_handle.Pass());
}

}  // namespace shell
}  // namespace mojo
