// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "mojo/service_manager/service_manager.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/interfaces/application/application.mojom.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/service_manager/service_loader.h"

namespace mojo {

namespace {
// Used by TestAPI.
bool has_created_instance = false;

class StubServiceProvider : public InterfaceImpl<ServiceProvider> {
 public:
  ServiceProvider* GetRemoteServiceProvider() {
    return client();
  }

 private:
  virtual void ConnectToService(
      const String& service_name,
      ScopedMessagePipeHandle client_handle) MOJO_OVERRIDE {}
};

}

class ServiceManager::ShellImpl : public InterfaceImpl<Shell> {
 public:
  ShellImpl(ServiceManager* manager, const GURL& url)
      : manager_(manager),
        url_(url) {
  }

  virtual ~ShellImpl() {
  }

  void ConnectToClient(const GURL& requestor_url,
                       ServiceProviderPtr service_provider) {
    client()->AcceptConnection(String::From(requestor_url),
                               service_provider.Pass());
  }

  // ServiceProvider implementation:
  virtual void ConnectToApplication(
      const String& app_url,
      InterfaceRequest<ServiceProvider> in_service_provider) OVERRIDE {
    ServiceProviderPtr out_service_provider;
    out_service_provider.Bind(in_service_provider.PassMessagePipe());
    manager_->ConnectToApplication(
        app_url.To<GURL>(),
        url_,
        out_service_provider.Pass());
  }

  const GURL& url() const { return url_; }

 private:
  virtual void OnConnectionError() OVERRIDE {
    manager_->OnShellImplError(this);
  }

  ServiceManager* const manager_;
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ShellImpl);
};

// static
ServiceManager::TestAPI::TestAPI(ServiceManager* manager) : manager_(manager) {
}

ServiceManager::TestAPI::~TestAPI() {
}

bool ServiceManager::TestAPI::HasCreatedInstance() {
  return has_created_instance;
}

bool ServiceManager::TestAPI::HasFactoryForURL(const GURL& url) const {
  return manager_->url_to_shell_impl_.find(url) !=
         manager_->url_to_shell_impl_.end();
}

ServiceManager::ServiceManager() : interceptor_(NULL) {
}

ServiceManager::~ServiceManager() {
  TerminateShellConnections();
  STLDeleteValues(&url_to_loader_);
  STLDeleteValues(&scheme_to_loader_);
}

void ServiceManager::TerminateShellConnections() {
  STLDeleteValues(&url_to_shell_impl_);
}

// static
ServiceManager* ServiceManager::GetInstance() {
  static base::LazyInstance<ServiceManager> instance =
      LAZY_INSTANCE_INITIALIZER;
  has_created_instance = true;
  return &instance.Get();
}

void ServiceManager::ConnectToApplication(const GURL& url,
                                          const GURL& requestor_url,
                                          ServiceProviderPtr service_provider) {
  URLToShellImplMap::const_iterator shell_it = url_to_shell_impl_.find(url);
  ShellImpl* shell_impl;
  if (shell_it != url_to_shell_impl_.end()) {
    shell_impl = shell_it->second;
  } else {
    MessagePipe pipe;
    GetLoaderForURL(url)->LoadService(this, url, pipe.handle0.Pass());
    shell_impl = WeakBindToPipe(new ShellImpl(this, url), pipe.handle1.Pass());
    url_to_shell_impl_[url] = shell_impl;
  }
  if (interceptor_) {
    shell_impl->ConnectToClient(
        requestor_url,
        interceptor_->OnConnectToClient(url, service_provider.Pass()));
  } else {
    shell_impl->ConnectToClient(requestor_url, service_provider.Pass());
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
  return default_loader_.get();
}

void ServiceManager::OnShellImplError(ShellImpl* shell_impl) {
  // Called from ~ShellImpl, so we do not need to call Destroy here.
  const GURL url = shell_impl->url();
  URLToShellImplMap::iterator it = url_to_shell_impl_.find(url);
  DCHECK(it != url_to_shell_impl_.end());
  delete it->second;
  url_to_shell_impl_.erase(it);
  ServiceLoader* loader = GetLoaderForURL(url);
  if (loader)
    loader->OnServiceError(this, url);
}

ScopedMessagePipeHandle ServiceManager::ConnectToServiceByName(
      const GURL& application_url,
      const std::string& interface_name) {
  StubServiceProvider* stub_sp = new StubServiceProvider;
  ServiceProviderPtr spp;
  BindToProxy(stub_sp, &spp);
  ConnectToApplication(application_url, GURL(), spp.Pass());
  MessagePipe pipe;
  stub_sp->GetRemoteServiceProvider()->ConnectToService(
      interface_name, pipe.handle1.Pass());
  return pipe.handle0.Pass();
}
}  // namespace mojo
