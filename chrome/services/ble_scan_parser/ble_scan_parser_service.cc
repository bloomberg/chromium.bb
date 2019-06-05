// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/ble_scan_parser/ble_scan_parser_service.h"
#include "chrome/services/ble_scan_parser/ble_scan_parser_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace ble_scan_parser {

BleScanParserService::BleScanParserService(
    service_manager::mojom::ServiceRequest request)
    : binding_(this, std::move(request)),
      keepalive_(&binding_, base::TimeDelta::FromSeconds(0)) {}

BleScanParserService::~BleScanParserService() = default;

void BleScanParserService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (interface_name == mojom::BleScanParser::Name_) {
    mojo::MakeStrongBinding(
        std::make_unique<BleScanParserImpl>(keepalive_.CreateRef()),
        ble_scan_parser::mojom::BleScanParserRequest(
            std::move(interface_pipe)));
  }
}

}  // namespace ble_scan_parser
