// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/geolocation_context.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/geolocation/geolocation_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

GeolocationContext::GeolocationContext() = default;

GeolocationContext::~GeolocationContext() = default;

// static
void GeolocationContext::Create(mojom::GeolocationContextRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<GeolocationContext>(),
                          std::move(request));
}

void GeolocationContext::BindGeolocation(mojom::GeolocationRequest request) {
  GeolocationImpl* impl = new GeolocationImpl(std::move(request), this);
  impls_.push_back(base::WrapUnique<GeolocationImpl>(impl));
  if (geoposition_override_)
    impl->SetOverride(*geoposition_override_);
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

void GeolocationContext::SetOverride(mojom::GeopositionPtr geoposition) {
  geoposition_override_ = std::move(geoposition);
  for (auto& impl : impls_) {
    impl->SetOverride(*geoposition_override_);
  }
}

void GeolocationContext::ClearOverride() {
  for (auto& impl : impls_) {
    impl->ClearOverride();
  }
}

}  // namespace device
