// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/geolocation_service_context.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/geolocation/geolocation_service_impl.h"

namespace device {

GeolocationServiceContext::GeolocationServiceContext() {}

GeolocationServiceContext::~GeolocationServiceContext() {}

void GeolocationServiceContext::CreateService(
    mojo::InterfaceRequest<mojom::GeolocationService> request) {
  GeolocationServiceImpl* service =
      new GeolocationServiceImpl(std::move(request), this);
  services_.push_back(base::WrapUnique<GeolocationServiceImpl>(service));
  if (geoposition_override_)
    service->SetOverride(*geoposition_override_.get());
  else
    service->StartListeningForUpdates();
}

void GeolocationServiceContext::ServiceHadConnectionError(
    GeolocationServiceImpl* service) {
  auto it =
      std::find_if(services_.begin(), services_.end(),
                   [service](const std::unique_ptr<GeolocationServiceImpl>& s) {
                     return service == s.get();
                   });
  DCHECK(it != services_.end());
  services_.erase(it);
}

void GeolocationServiceContext::SetOverride(
    std::unique_ptr<Geoposition> geoposition) {
  geoposition_override_.swap(geoposition);
  for (auto& service : services_) {
    service->SetOverride(*geoposition_override_.get());
  }
}

void GeolocationServiceContext::ClearOverride() {
  for (auto& service : services_) {
    service->ClearOverride();
  }
}

}  // namespace device
