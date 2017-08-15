// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_GEOLOCATION_CONFIG_H_
#define DEVICE_GEOLOCATION_GEOLOCATION_CONFIG_H_

#include "base/compiler_specific.h"
#include "device/geolocation/geolocation_export.h"
#include "device/geolocation/geolocation_provider_impl.h"
#include "device/geolocation/public/interfaces/geolocation_config.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace device {

// Implements the GeolocationConfig Mojo interface.
class DEVICE_GEOLOCATION_EXPORT GeolocationConfig
    : public mojom::GeolocationConfig {
 public:
  GeolocationConfig();
  ~GeolocationConfig() override;

  static void Create(mojom::GeolocationConfigRequest request,
                     const service_manager::BindSourceInfo& source_info);

  void IsHighAccuracyLocationBeingCaptured(
      IsHighAccuracyLocationBeingCapturedCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationConfig);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_GEOLOCATION_CONFIG_H_
