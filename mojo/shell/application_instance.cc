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

// It's valid to specify mojo: URLs in the filter either as mojo:foo or
// mojo://foo/ - but we store the filter in the latter form.
CapabilityFilter CanonicalizeFilter(const CapabilityFilter& filter) {
  CapabilityFilter canonicalized;
  for (CapabilityFilter::const_iterator it = filter.begin();
       it != filter.end();
       ++it) {
    if (it->first == "*")
      canonicalized[it->first] = it->second;
    else
      canonicalized[GURL(it->first).spec()] = it->second;
  }
  return canonicalized;
}

ApplicationInstance::QueuedClientRequest::QueuedClientRequest()
    : originator(nullptr) {}

ApplicationInstance::QueuedClientRequest::~QueuedClientRequest() {
}

ApplicationInstance::ApplicationInstance(
    ApplicationPtr application,
    ApplicationManager* manager,
    const Identity& originator_identity,
    const Identity& identity,
    const CapabilityFilter& filter,
    const base::Closure& on_application_end)
    : manager_(manager),
      originator_identity_(originator_identity),
      identity_(identity),
      filter_(CanonicalizeFilter(filter)),
      allow_any_application_(filter.size() == 1 && filter.count("*") == 1),
      on_application_end_(on_application_end),
      application_(application.Pass()),
      binding_(this),
      queue_requests_(false) {
  binding_.set_connection_error_handler([this]() { OnConnectionError(); });
}

ApplicationInstance::~ApplicationInstance() {
  STLDeleteElements(&queued_client_requests_);
}

void ApplicationInstance::InitializeApplication() {
  ShellPtr shell;
  binding_.Bind(GetProxy(&shell));
  application_->Initialize(shell.Pass(), identity_.url.spec());
}

void ApplicationInstance::ConnectToClient(
    ApplicationInstance* originator,
    const GURL& requested_url,
    const GURL& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    const CapabilityFilter& filter) {
  if (queue_requests_) {
    QueuedClientRequest* queued_request = new QueuedClientRequest();
    queued_request->originator = originator;
    queued_request->requested_url = requested_url;
    queued_request->requestor_url = requestor_url;
    queued_request->services = services.Pass();
    queued_request->exposed_services = exposed_services.Pass();
    queued_request->filter = filter;
    queued_client_requests_.push_back(queued_request);
    return;
  }

  CallAcceptConnection(originator, requestor_url, services.Pass(),
                       exposed_services.Pass(), requested_url);
}

AllowedInterfaces ApplicationInstance::GetAllowedInterfaces(
    const Identity& identity) const {
  // Start by looking for interfaces specific to the supplied identity.
  auto it = filter_.find(identity.url.spec());
  if (it != filter_.end())
    return it->second;

  // Fall back to looking for a wildcard rule.
  it = filter_.find("*");
  if (filter_.size() == 1 && it != filter_.end())
    return it->second;

  // Finally, nothing is allowed.
  return AllowedInterfaces();
}

// Shell implementation:
void ApplicationInstance::ConnectToApplication(
    URLRequestPtr app_request,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    CapabilityFilterPtr filter) {
  std::string url_string = app_request->url.To<std::string>();
  GURL url(url_string);
  if (!url.is_valid()) {
    LOG(ERROR) << "Error: invalid URL: " << url_string;
    return;
  }
  if (allow_any_application_ || filter_.find(url.spec()) != filter_.end()) {
    CapabilityFilter capability_filter = GetPermissiveCapabilityFilter();
    if (!filter.is_null())
      capability_filter = filter->filter.To<CapabilityFilter>();
    manager_->ConnectToApplication(this, app_request.Pass(), std::string(),
                                   identity_.url, services.Pass(),
                                   exposed_services.Pass(), capability_filter,
                                   base::Closure());
  } else {
    LOG(WARNING) << "CapabilityFilter prevented connection from: " <<
        identity_.url << " to: " << url.spec();
  }
}

void ApplicationInstance::QuitApplication() {
  queue_requests_ = true;
  application_->OnQuitRequested(
      base::Bind(&ApplicationInstance::OnQuitRequestedResult,
                 base::Unretained(this)));
}

void ApplicationInstance::CallAcceptConnection(
    ApplicationInstance* originator,
    const GURL& requestor_url,
    InterfaceRequest<ServiceProvider> services,
    ServiceProviderPtr exposed_services,
    const GURL& requested_url) {
  AllowedInterfaces interfaces;
  interfaces.insert("*");
  if (originator)
    interfaces = originator->GetAllowedInterfaces(identity_);
  application_->AcceptConnection(requestor_url.spec(),
                                 services.Pass(),
                                 exposed_services.Pass(),
                                 Array<String>::From(interfaces).Pass(),
                                 requested_url.spec());
}

void ApplicationInstance::OnConnectionError() {
  std::vector<QueuedClientRequest*> queued_client_requests;
  queued_client_requests_.swap(queued_client_requests);
  auto manager = manager_;
  manager_->OnApplicationInstanceError(this);
  //|this| is deleted.

  // If any queued requests came to shell during time it was shutting down,
  // start them now.
  for (auto request : queued_client_requests) {
    mojo::URLRequestPtr url(mojo::URLRequest::New());
    url->url = mojo::String::From(request->requested_url.spec());
    ApplicationInstance* originator =
        manager->GetApplicationInstance(originator_identity_);
    manager->ConnectToApplication(originator, url.Pass(), std::string(),
                                  request->requestor_url,
                                  request->services.Pass(),
                                  request->exposed_services.Pass(),
                                  request->filter,
                                  base::Closure());
  }
  STLDeleteElements(&queued_client_requests);
}

void ApplicationInstance::OnQuitRequestedResult(bool can_quit) {
  if (can_quit)
    return;

  queue_requests_ = false;
  for (auto request : queued_client_requests_) {
    CallAcceptConnection(request->originator,
                         request->requestor_url,
                         request->services.Pass(),
                         request->exposed_services.Pass(),
                         request->requested_url);
  }
  STLDeleteElements(&queued_client_requests_);
}

}  // namespace shell
}  // namespace mojo
