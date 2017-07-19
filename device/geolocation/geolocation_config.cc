// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/geolocation_config.h"

#include "base/bind.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

GeolocationConfig::GeolocationConfig() {}

GeolocationConfig::~GeolocationConfig() {}

// static
void GeolocationConfig::Create(
    mojom::GeolocationConfigRequest request,
    const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(base::MakeUnique<GeolocationConfig>(),
                          std::move(request));
}

void GeolocationConfig::IsHighAccuracyLocationBeingCaptured(
    IsHighAccuracyLocationBeingCapturedCallback callback) {
  std::move(callback).Run(
      GeolocationProvider::GetInstance()->HighAccuracyLocationInUse());
}

}  // namespace device
