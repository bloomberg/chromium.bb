// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/application_instance.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/shell/application_manager.h"

namespace mojo {
namespace shell {

ApplicationInstance::ApplicationInstance(
    ApplicationPtr application,
    ApplicationManager* manager,
    const Identity& identity,
    uint32_t requesting_content_handler_id,
    const base::Closure& on_application_end)
    : manager_(manager),
      identity_(identity),
      allow_any_application_(identity.filter().size() == 1 &&
                             identity.filter().count("*") == 1),
      requesting_content_handler_id_(requesting_content_handler_id),
      on_application_end_(on_application_end),
      application_(application.Pass()),
      binding_(this),
      queue_requests_(false) {
  binding_.set_connection_error_handler([this]() { OnConnectionError(); });
}

ApplicationInstance::~ApplicationInstance() {
  for (auto request : queued_client_requests_)
    request->connect_callback().Run(kInvalidContentHandlerID);
  STLDeleteElements(&queued_client_requests_);
}

void ApplicationInstance::InitializeApplication() {
  ShellPtr shell;
  binding_.Bind(GetProxy(&shell));
  application_->Initialize(shell.Pass(), identity_.url().spec());
}

void ApplicationInstance::ConnectToClient(
    scoped_ptr<ConnectToApplicationParams> params) {
  if (queue_requests_) {
    queued_client_requests_.push_back(params.release());
    return;
  }

  CallAcceptConnection(params.Pass());
}

// Shell implementation:
void ApplicationInstance::ConnectToApplication(
    URLRequestPtr app_request,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    CapabilityFilterPtr filter,
    const ConnectToApplicationCallback& callback) {
  std::string url_string = app_request->url.To<std::string>();
  GURL url(url_string);
  if (!url.is_valid()) {
    LOG(ERROR) << "Error: invalid URL: " << url_string;
    callback.Run(kInvalidContentHandlerID);
    return;
  }
  if (allow_any_application_ ||
      identity_.filter().find(url.spec()) != identity_.filter().end()) {
    CapabilityFilter capability_filter = GetPermissiveCapabilityFilter();
    if (!filter.is_null())
      capability_filter = filter->filter.To<CapabilityFilter>();

    scoped_ptr<ConnectToApplicationParams> params(
        new ConnectToApplicationParams);
    params->SetSource(this);
    GURL app_url(app_request->url);
    params->SetTargetURLRequest(
        app_request.Pass(),
        Identity(app_url, std::string(), capability_filter));
    params->set_services(services.Pass());
    params->set_exposed_services(exposed_services.Pass());
    params->set_connect_callback(callback);
    manager_->ConnectToApplication(params.Pass());
  } else {
    LOG(WARNING) << "CapabilityFilter prevented connection from: " <<
        identity_.url() << " to: " << url.spec();
    callback.Run(kInvalidContentHandlerID);
  }
}

void ApplicationInstance::QuitApplication() {
  queue_requests_ = true;
  application_->OnQuitRequested(
      base::Bind(&ApplicationInstance::OnQuitRequestedResult,
                 base::Unretained(this)));
}

void ApplicationInstance::CallAcceptConnection(
    scoped_ptr<ConnectToApplicationParams> params) {
  params->connect_callback().Run(requesting_content_handler_id_);
  AllowedInterfaces interfaces;
  interfaces.insert("*");
  if (!params->source().is_null())
    interfaces = GetAllowedInterfaces(params->source().filter(), identity_);

  application_->AcceptConnection(
      params->source().url().spec(), params->TakeServices(),
      params->TakeExposedServices(), Array<String>::From(interfaces).Pass(),
      params->target().url().spec());
}

void ApplicationInstance::OnConnectionError() {
  std::vector<ConnectToApplicationParams*> queued_client_requests;
  queued_client_requests_.swap(queued_client_requests);
  auto manager = manager_;
  manager_->OnApplicationInstanceError(this);
  //|this| is deleted.

  // If any queued requests came to shell during time it was shutting down,
  // start them now.
  for (auto request : queued_client_requests) {
    // Unfortunately, it is possible that |request->target_url_request()| is
    // null at this point. Consider the following sequence:
    // 1) connect_request_1 arrives at the application manager; the manager
    //    decides to fetch the app.
    // 2) connect_request_2 arrives for the same app; because the app is not
    //    running yet, the manager decides to fetch the app again.
    // 3) The fetch for step (1) completes and an application instance app_a is
    //    registered.
    // 4) app_a goes into two-phase shutdown.
    // 5) The fetch for step (2) completes; the manager finds that there is a
    //    running app already, so it connects to app_a.
    // 6) connect_request_2 is queued (and eventually gets here), but its
    //    original_request field was already lost to NetworkFetcher at step (2).
    //
    // TODO(yzshen): It seems we should register a pending application instance
    // before starting the fetch. So at step (2) the application manager knows
    // that it can wait for the first fetch to complete instead of doing a
    // second one directly.
    if (!request->target_url_request()) {
      URLRequestPtr url_request = mojo::URLRequest::New();
      url_request->url = request->target().url().spec();
      request->SetTargetURLRequest(url_request.Pass(), request->target());
    }
    manager->ConnectToApplication(make_scoped_ptr(request));
  }
}

void ApplicationInstance::OnQuitRequestedResult(bool can_quit) {
  if (can_quit)
    return;

  queue_requests_ = false;
  for (auto request : queued_client_requests_)
    CallAcceptConnection(make_scoped_ptr(request));

  queued_client_requests_.clear();
}

}  // namespace shell
}  // namespace mojo
