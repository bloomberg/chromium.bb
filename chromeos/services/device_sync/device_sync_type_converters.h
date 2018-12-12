// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_TYPE_CONVERTERS_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_TYPE_CONVERTERS_H_

#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<chromeos::device_sync::mojom::NetworkRequestResult,
                     chromeos::device_sync::NetworkRequestError> {
  static chromeos::device_sync::mojom::NetworkRequestResult Convert(
      chromeos::device_sync::NetworkRequestError type);
};

}  // namespace mojo

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_TYPE_CONVERTERS_H_
