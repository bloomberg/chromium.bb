// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_BLE_SCAN_PARSER_BLE_SCAN_PARSER_SERVICE_H_
#define CHROME_SERVICES_BLE_SCAN_PARSER_BLE_SCAN_PARSER_SERVICE_H_

#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"

#include "base/macros.h"
#include "chrome/services/ble_scan_parser/public/mojom/ble_scan_parser.mojom.h"

namespace ble_scan_parser {

class BleScanParserService : public service_manager::Service {
 public:
  explicit BleScanParserService(service_manager::mojom::ServiceRequest request);
  ~BleScanParserService() override;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceBinding binding_;
  service_manager::ServiceKeepalive keepalive_;

  DISALLOW_COPY_AND_ASSIGN(BleScanParserService);
};

}  // namespace ble_scan_parser

#endif  // CHROME_SERVICES_BLE_SCAN_PARSER_BLE_SCAN_PARSER_SERVICE_H_
