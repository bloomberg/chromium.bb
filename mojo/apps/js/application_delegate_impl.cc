// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/application_delegate_impl.h"

#include "mojo/apps/js/js_app.h"
#include "mojo/public/cpp/application/application_impl.h"

namespace mojo {
namespace apps {

ContentHandlerImpl::ContentHandlerImpl(ApplicationDelegateImpl* app)
    : content_handler_(app) {
}

ContentHandlerImpl::~ContentHandlerImpl() {
}

void ContentHandlerImpl::OnConnect(
    const mojo::String& url,
    URLResponsePtr content,
    InterfaceRequest<ServiceProvider> service_provider) {
  content_handler_->StartJSApp(url.To<std::string>(), content.Pass());
}

ApplicationDelegateImpl::ApplicationDelegateImpl()
    : application_impl_(NULL), content_handler_factory_(this) {
}

void ApplicationDelegateImpl::Initialize(ApplicationImpl* app) {
  application_impl_ = app;
}

ApplicationDelegateImpl::~ApplicationDelegateImpl() {
}

bool ApplicationDelegateImpl::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService(&content_handler_factory_);
  return true;
}

void ApplicationDelegateImpl::StartJSApp(const std::string& url,
                                         URLResponsePtr content) {
  JSApp* app = new JSApp(this, url, content.Pass());
  app_vector_.push_back(app);
  // TODO(hansmuller): deal with the Start() return value.
  app->Start();
}

void ApplicationDelegateImpl::QuitJSApp(JSApp* app) {
  AppVector::iterator itr =
      std::find(app_vector_.begin(), app_vector_.end(), app);
  if (itr != app_vector_.end())
    app_vector_.erase(itr);
}

void ApplicationDelegateImpl::ConnectToService(
    ScopedMessagePipeHandle pipe_handle,
    const std::string& application_url,
    const std::string& interface_name) {
  CHECK(application_impl_);
  ServiceProvider* service_provider =
      application_impl_->ConnectToApplication(application_url)
          ->GetServiceProvider();
  service_provider->ConnectToService(interface_name, pipe_handle.Pass());
}

}  // namespace apps
}  // namespace mojo
