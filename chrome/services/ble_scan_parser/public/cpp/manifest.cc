// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/ble_scan_parser/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/ble_scan_parser/public/mojom/ble_scan_parser.mojom.h"
#include "chrome/services/ble_scan_parser/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace ble_scan_parser {

const service_manager::Manifest GetManifest() {
  return service_manager::ManifestBuilder()
      .WithServiceName(mojom::kServiceName)
      .WithDisplayName(IDS_UTILITY_PROCESS_BLE_SCAN_PARSER_NAME)
      .ExposeCapability(
          mojom::kParseBleScanCapability,
          service_manager::Manifest::InterfaceList<mojom::BleScanParser>())
      .Build();
}

}  // namespace ble_scan_parser
