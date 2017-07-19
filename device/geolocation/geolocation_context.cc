// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/geolocation_context.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/geolocation/geolocation_impl.h"

namespace device {

GeolocationContext::GeolocationContext() {}

GeolocationContext::~GeolocationContext() {}

void GeolocationContext::Bind(mojom::GeolocationRequest request) {
  GeolocationImpl* impl = new GeolocationImpl(std::move(request), this);
  impls_.push_back(base::WrapUnique<GeolocationImpl>(impl));
  if (geoposition_override_)
    impl->SetOverride(*geoposition_override_.get());
  else
    impl->StartListeningForUpdates();
}

void GeolocationContext::OnConnectionError(GeolocationImpl* impl) {
  auto it = std::find_if(impls_.begin(), impls_.end(),
                         [impl](const std::unique_ptr<GeolocationImpl>& gi) {
                           return impl == gi.get();
                         });
  DCHECK(it != impls_.end());
  impls_.erase(it);
}

void GeolocationContext::SetOverride(std::unique_ptr<Geoposition> geoposition) {
  geoposition_override_.swap(geoposition);
  for (auto& impl : impls_) {
    impl->SetOverride(*geoposition_override_.get());
  }
}

void GeolocationContext::ClearOverride() {
  for (auto& impl : impls_) {
    impl->ClearOverride();
  }
}

}  // namespace device
