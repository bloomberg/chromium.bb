// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/mojom/cros_network_config_mojom_traits.h"

#include "base/logging.h"

namespace mojo {

chromeos::network_config::mojom::ONCSource
EnumTraits<chromeos::network_config::mojom::ONCSource, onc::ONCSource>::ToMojom(
    onc::ONCSource input) {
  switch (input) {
    case onc::ONC_SOURCE_UNKNOWN:
      return chromeos::network_config::mojom::ONCSource::kUnknown;
    case onc::ONC_SOURCE_NONE:
      return chromeos::network_config::mojom::ONCSource::kNone;
    case onc::ONC_SOURCE_USER_IMPORT:
      return chromeos::network_config::mojom::ONCSource::kUserImport;
    case onc::ONC_SOURCE_DEVICE_POLICY:
      return chromeos::network_config::mojom::ONCSource::kDevicePolicy;
    case onc::ONC_SOURCE_USER_POLICY:
      return chromeos::network_config::mojom::ONCSource::kUserPolicy;
  }

  NOTREACHED();
  return chromeos::network_config::mojom::ONCSource::kUnknown;
}

bool EnumTraits<chromeos::network_config::mojom::ONCSource, onc::ONCSource>::
    FromMojom(chromeos::network_config::mojom::ONCSource input,
              onc::ONCSource* out) {
  switch (input) {
    case chromeos::network_config::mojom::ONCSource::kUnknown:
      *out = onc::ONC_SOURCE_UNKNOWN;
      return true;
    case chromeos::network_config::mojom::ONCSource::kNone:
      *out = onc::ONC_SOURCE_NONE;
      return true;
    case chromeos::network_config::mojom::ONCSource::kUserImport:
      *out = onc::ONC_SOURCE_USER_IMPORT;
      return true;
    case chromeos::network_config::mojom::ONCSource::kDevicePolicy:
      *out = onc::ONC_SOURCE_DEVICE_POLICY;
      return true;
    case chromeos::network_config::mojom::ONCSource::kUserPolicy:
      *out = onc::ONC_SOURCE_USER_POLICY;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
