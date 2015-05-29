// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/shell/application_manager.h"

namespace mojo {
namespace shell {

ShellImpl::QueuedClientRequest::QueuedClientRequest() {
}

ShellImpl::QueuedClientRequest::~QueuedClientRequest() {
}

ShellImpl::ShellImpl(ApplicationPtr application,
                     ApplicationManager* manager,
                     const Identity& identity,
                     const base::Closure& on_application_end)
    : manager_(manager),
      identity_(identity),
      on_application_end_(on_application_end),
      application_(application.Pass()),
      binding_(this),
      queue_requests_(false) {
  binding_.set_error_handler(this);
}

ShellImpl::~ShellImpl() {
  STLDeleteElements(&queued_client_requests_);
}

void ShellImpl::InitializeApplication() {
  ShellPtr shell;
  binding_.Bind(GetProxy(&shell));
  application_->Initialize(shell.Pass(), identity_.url.spec());
}

void ShellImpl::ConnectToClient(const GURL& requested_url,
                                const GURL& requestor_url,
                                InterfaceRequest<ServiceProvider> services,
                                ServiceProviderPtr exposed_services) {
  if (queue_requests_) {
    QueuedClientRequest* queued_request = new QueuedClientRequest;
    queued_request->requested_url = requested_url;
    queued_request->requestor_url = requestor_url;
    queued_request->services = services.Pass();
    queued_request->exposed_services = exposed_services.Pass();
    queued_client_requests_.push_back(queued_request);
    return;
  }

  application_->AcceptConnection(requestor_url.spec(), services.Pass(),
                                 exposed_services.Pass(), requested_url.spec());
}

// Shell implementation:
void ShellImpl::ConnectToApplication(mojo::URLRequestPtr app_request,
                                     InterfaceRequest<ServiceProvider> services,
                                     ServiceProviderPtr exposed_services) {
  GURL app_gurl(app_request->url.To<std::string>());
  if (!app_gurl.is_valid()) {
    LOG(ERROR) << "Error: invalid URL: " << app_request;
    return;
  }
  manager_->ConnectToApplication(app_request.Pass(), identity_.url,
                                 services.Pass(), exposed_services.Pass(),
                                 base::Closure());
}

void ShellImpl::QuitApplication() {
  queue_requests_ = true;
  application_->OnQuitRequested(base::Bind(&ShellImpl::OnQuitRequestedResult,
                                           base::Unretained(this)));
}

void ShellImpl::OnConnectionError() {
  std::vector<QueuedClientRequest*> queued_client_requests;
  queued_client_requests_.swap(queued_client_requests);
  auto manager = manager_;
  manager_->OnShellImplError(this);
  //|this| is deleted.

  // If any queued requests came to shell during time it was shutting down,
  // start them now.
  for (auto request : queued_client_requests) {
    mojo::URLRequestPtr url(mojo::URLRequest::New());
    url->url = mojo::String::From(request->requested_url.spec());
    manager->ConnectToApplication(url.Pass(), request->requestor_url,
                                  request->services.Pass(),
                                  request->exposed_services.Pass(),
                                  base::Closure());
  }
  STLDeleteElements(&queued_client_requests);
}

void ShellImpl::OnQuitRequestedResult(bool can_quit) {
  if (can_quit)
    return;

  queue_requests_ = false;
  for (auto request : queued_client_requests_) {
    application_->AcceptConnection(request->requestor_url.spec(),
                                   request->services.Pass(),
                                   request->exposed_services.Pass(),
                                   request->requested_url.spec());
  }
  STLDeleteElements(&queued_client_requests_);
}

}  // namespace shell
}  // namespace mojo
