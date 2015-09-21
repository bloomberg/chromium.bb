// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CONNECT_TO_APPLICATION_PARAMS_H_
#define MOJO_SHELL_CONNECT_TO_APPLICATION_PARAMS_H_

#include <string>

#include "base/callback.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/identity.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ApplicationInstance;

// This class represents a request for the application manager to connect to an
// application.
class ConnectToApplicationParams {
 public:
  ConnectToApplicationParams();
  ~ConnectToApplicationParams();

  // Sets both |originator_identity_| and |originator_filter_|. If |originator|
  // is null, both fields are reset.
  void SetOriginatorInfo(ApplicationInstance* originator);

  // Sets both |app_url_| and |app_url_request_|.
  void SetURLInfo(const GURL& app_url);
  // Sets both |app_url_| and |app_url_request_|.
  void SetURLInfo(URLRequestPtr app_url_request);

  void set_originator_identity(const Identity& value) {
    originator_identity_ = value;
  }
  const Identity& originator_identity() const { return originator_identity_; }

  void set_originator_filter(const CapabilityFilter& value) {
    originator_filter_ = value;
  }
  const CapabilityFilter& originator_filter() const {
    return originator_filter_;
  }

  const GURL& app_url() const { return app_url_; }

  const URLRequest* app_url_request() const { return app_url_request_.get(); }
  // NOTE: This doesn't reset |app_url_|.
  URLRequestPtr TakeAppURLRequest() { return app_url_request_.Pass(); }

  void set_qualifier(const std::string& value) { qualifier_ = value; }
  const std::string& qualifier() const { return qualifier_; }

  void set_services(InterfaceRequest<ServiceProvider> value) {
    services_ = value.Pass();
  }
  InterfaceRequest<ServiceProvider> TakeServices() { return services_.Pass(); }

  void set_exposed_services(ServiceProviderPtr value) {
    exposed_services_ = value.Pass();
  }
  ServiceProviderPtr TakeExposedServices() { return exposed_services_.Pass(); }

  void set_filter(const CapabilityFilter& value) { filter_ = value; }
  const CapabilityFilter& filter() const { return filter_; }

  void set_on_application_end(const base::Closure& value) {
    on_application_end_ = value;
  }
  const base::Closure& on_application_end() const {
    return on_application_end_;
  }

  void set_connect_callback(const Shell::ConnectToApplicationCallback& value) {
    connect_callback_ = value;
  }
  const Shell::ConnectToApplicationCallback& connect_callback() const {
    return connect_callback_;
  }

 private:
  // It may be null (i.e., is_null() returns true) which indicates that there is
  // no originator (e.g., for the first application or in tests).
  Identity originator_identity_;
  // Should be ignored if |originator_identity_| is null.
  CapabilityFilter originator_filter_;
  // The URL of the application that is being connected to.
  GURL app_url_;
  // The URL request to fetch the application. It may contain more information
  // than |app_url_| (e.g., headers, request body). When it is taken, |app_url_|
  // remains unchanged.
  URLRequestPtr app_url_request_;
  // Please see the comments in identity.h for the exact meaning of qualifier.
  std::string qualifier_;
  InterfaceRequest<ServiceProvider> services_;
  ServiceProviderPtr exposed_services_;
  CapabilityFilter filter_;
  base::Closure on_application_end_;
  Shell::ConnectToApplicationCallback connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(ConnectToApplicationParams);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_CONNECT_TO_APPLICATION_PARAMS_H_
