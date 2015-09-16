// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/core_services/application_delegate_factory.h"

#include "components/mus/mus_app.h"
#include "components/resource_provider/resource_provider_app.h"
#include "mojo/services/network/network_service_delegate.h"

namespace core_services {

scoped_ptr<mojo::ApplicationDelegate> CreateApplicationDelegateNotAndroid(
    const std::string& url) {
  if (url == "mojo://network_service/")
    return make_scoped_ptr(new mojo::NetworkServiceDelegate);
  if (url == "mojo://resource_provider/") {
    return make_scoped_ptr(
        new resource_provider::ResourceProviderApp("mojo:core_services"));
  }
  if (url == "mojo://mus/")
    return make_scoped_ptr(new mus::MandolineUIServicesApp);
  return nullptr;
}

}  // namespace core_services
