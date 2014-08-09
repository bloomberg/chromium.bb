// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application_manager/application_manager.h"

#include <stdio.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "mojo/application_manager/application_loader.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/interfaces/application/application.mojom.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/public/interfaces/content_handler/content_handler.mojom.h"

namespace mojo {

namespace {
// Used by TestAPI.
bool has_created_instance = false;

class StubServiceProvider : public InterfaceImpl<ServiceProvider> {
 public:
  ServiceProvider* GetRemoteServiceProvider() { return client(); }

 private:
  virtual void ConnectToService(const String& service_name,
                                ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE {}
};

}  // namespace

class ApplicationManager::LoadCallbacksImpl
    : public ApplicationLoader::LoadCallbacks {
 public:
  LoadCallbacksImpl(base::WeakPtr<ApplicationManager> manager,
                    const GURL& requested_url,
                    const GURL& requestor_url,
                    ServiceProviderPtr service_provider)
      : manager_(manager),
        requested_url_(requested_url),
        requestor_url_(requestor_url),
        service_provider_(service_provider.Pass()) {}

 private:
  virtual ~LoadCallbacksImpl() {}

  // LoadCallbacks implementation
  virtual ScopedMessagePipeHandle RegisterApplication() OVERRIDE {
    ScopedMessagePipeHandle shell_handle;
    if (manager_) {
      manager_->RegisterLoadedApplication(requested_url_,
                                          requestor_url_,
                                          service_provider_.Pass(),
                                          &shell_handle);
    }
    return shell_handle.Pass();
  }

  virtual void LoadWithContentHandler(const GURL& content_handler_url,
                                      URLResponsePtr content) OVERRIDE {
    if (manager_) {
      manager_->LoadWithContentHandler(requested_url_,
                                       requestor_url_,
                                       content_handler_url,
                                       content.Pass(),
                                       service_provider_.Pass());
    }
  }

  base::WeakPtr<ApplicationManager> manager_;
  GURL requested_url_;
  GURL requestor_url_;
  ServiceProviderPtr service_provider_;
};

class ApplicationManager::ShellImpl : public InterfaceImpl<Shell> {
 public:
  ShellImpl(ApplicationManager* manager, const GURL& url)
      : manager_(manager), url_(url) {}

  virtual ~ShellImpl() {}

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
        app_url.To<GURL>(), url_, out_service_provider.Pass());
  }

  const GURL& url() const { return url_; }

 private:
  virtual void OnConnectionError() OVERRIDE {
    manager_->OnShellImplError(this);
  }

  ApplicationManager* const manager_;
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ShellImpl);
};

struct ApplicationManager::ContentHandlerConnection {
  ContentHandlerConnection(ApplicationManager* manager,
                           const GURL& content_handler_url) {
    ServiceProviderPtr service_provider;
    BindToProxy(&service_provider_impl, &service_provider);
    manager->ConnectToApplication(
        content_handler_url, GURL(), service_provider.Pass());
    mojo::ConnectToService(service_provider_impl.client(), &content_handler);
  }

  StubServiceProvider service_provider_impl;
  ContentHandlerPtr content_handler;
};

// static
ApplicationManager::TestAPI::TestAPI(ApplicationManager* manager)
    : manager_(manager) {
}

ApplicationManager::TestAPI::~TestAPI() {
}

bool ApplicationManager::TestAPI::HasCreatedInstance() {
  return has_created_instance;
}

bool ApplicationManager::TestAPI::HasFactoryForURL(const GURL& url) const {
  return manager_->url_to_shell_impl_.find(url) !=
         manager_->url_to_shell_impl_.end();
}

ApplicationManager::ApplicationManager()
    : interceptor_(NULL), weak_ptr_factory_(this) {
}

ApplicationManager::~ApplicationManager() {
  STLDeleteValues(&url_to_content_handler_);
  TerminateShellConnections();
  STLDeleteValues(&url_to_loader_);
  STLDeleteValues(&scheme_to_loader_);
}

void ApplicationManager::TerminateShellConnections() {
  STLDeleteValues(&url_to_shell_impl_);
}

// static
ApplicationManager* ApplicationManager::GetInstance() {
  static base::LazyInstance<ApplicationManager> instance =
      LAZY_INSTANCE_INITIALIZER;
  has_created_instance = true;
  return &instance.Get();
}

void ApplicationManager::ConnectToApplication(
    const GURL& url,
    const GURL& requestor_url,
    ServiceProviderPtr service_provider) {
  URLToShellImplMap::const_iterator shell_it = url_to_shell_impl_.find(url);
  if (shell_it != url_to_shell_impl_.end()) {
    ConnectToClient(
        shell_it->second, url, requestor_url, service_provider.Pass());
    return;
  }

  scoped_refptr<LoadCallbacksImpl> callbacks(
      new LoadCallbacksImpl(weak_ptr_factory_.GetWeakPtr(),
                            url,
                            requestor_url,
                            service_provider.Pass()));
  GetLoaderForURL(url)->Load(this, url, callbacks);
}

void ApplicationManager::ConnectToClient(ShellImpl* shell_impl,
                                         const GURL& url,
                                         const GURL& requestor_url,
                                         ServiceProviderPtr service_provider) {
  if (interceptor_) {
    shell_impl->ConnectToClient(
        requestor_url,
        interceptor_->OnConnectToClient(url, service_provider.Pass()));
  } else {
    shell_impl->ConnectToClient(requestor_url, service_provider.Pass());
  }
}

void ApplicationManager::RegisterLoadedApplication(
    const GURL& url,
    const GURL& requestor_url,
    ServiceProviderPtr service_provider,
    ScopedMessagePipeHandle* shell_handle) {
  ShellImpl* shell_impl = NULL;
  URLToShellImplMap::iterator iter = url_to_shell_impl_.find(url);
  if (iter != url_to_shell_impl_.end()) {
    // This can happen because services are loaded asynchronously. So if we get
    // two requests for the same service close to each other, we might get here
    // and find that we already have it.
    shell_impl = iter->second;
  } else {
    MessagePipe pipe;
    shell_impl = WeakBindToPipe(new ShellImpl(this, url), pipe.handle1.Pass());
    url_to_shell_impl_[url] = shell_impl;
    *shell_handle = pipe.handle0.Pass();
  }

  ConnectToClient(shell_impl, url, requestor_url, service_provider.Pass());
}

void ApplicationManager::LoadWithContentHandler(
    const GURL& content_url,
    const GURL& requestor_url,
    const GURL& content_handler_url,
    URLResponsePtr content,
    ServiceProviderPtr service_provider) {
  ContentHandlerConnection* connection = NULL;
  URLToContentHandlerMap::iterator iter =
      url_to_content_handler_.find(content_handler_url);
  if (iter != url_to_content_handler_.end()) {
    connection = iter->second;
  } else {
    connection = new ContentHandlerConnection(this, content_handler_url);
    url_to_content_handler_[content_handler_url] = connection;
  }
  connection->content_handler->OnConnect(
      content_url.spec(), content.Pass(), service_provider.Pass());
}

void ApplicationManager::SetLoaderForURL(scoped_ptr<ApplicationLoader> loader,
                                         const GURL& url) {
  URLToLoaderMap::iterator it = url_to_loader_.find(url);
  if (it != url_to_loader_.end())
    delete it->second;
  url_to_loader_[url] = loader.release();
}

void ApplicationManager::SetLoaderForScheme(
    scoped_ptr<ApplicationLoader> loader,
    const std::string& scheme) {
  SchemeToLoaderMap::iterator it = scheme_to_loader_.find(scheme);
  if (it != scheme_to_loader_.end())
    delete it->second;
  scheme_to_loader_[scheme] = loader.release();
}

void ApplicationManager::SetInterceptor(Interceptor* interceptor) {
  interceptor_ = interceptor;
}

ApplicationLoader* ApplicationManager::GetLoaderForURL(const GURL& url) {
  URLToLoaderMap::const_iterator url_it = url_to_loader_.find(url);
  if (url_it != url_to_loader_.end())
    return url_it->second;
  SchemeToLoaderMap::const_iterator scheme_it =
      scheme_to_loader_.find(url.scheme());
  if (scheme_it != scheme_to_loader_.end())
    return scheme_it->second;
  return default_loader_.get();
}

void ApplicationManager::OnShellImplError(ShellImpl* shell_impl) {
  // Called from ~ShellImpl, so we do not need to call Destroy here.
  const GURL url = shell_impl->url();
  URLToShellImplMap::iterator it = url_to_shell_impl_.find(url);
  DCHECK(it != url_to_shell_impl_.end());
  delete it->second;
  url_to_shell_impl_.erase(it);
  ApplicationLoader* loader = GetLoaderForURL(url);
  if (loader)
    loader->OnServiceError(this, url);
}

ScopedMessagePipeHandle ApplicationManager::ConnectToServiceByName(
    const GURL& application_url,
    const std::string& interface_name) {
  StubServiceProvider* stub_sp = new StubServiceProvider;
  ServiceProviderPtr spp;
  BindToProxy(stub_sp, &spp);
  ConnectToApplication(application_url, GURL(), spp.Pass());
  MessagePipe pipe;
  stub_sp->GetRemoteServiceProvider()->ConnectToService(interface_name,
                                                        pipe.handle1.Pass());
  return pipe.handle0.Pass();
}
}  // namespace mojo
