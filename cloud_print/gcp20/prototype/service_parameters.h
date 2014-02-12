// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_SERVICE_PARAMETERS_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_SERVICE_PARAMETERS_H_

#include <string>

#include "net/base/net_util.h"

// Stores information about service.
struct ServiceParameters {
  ServiceParameters();

  ~ServiceParameters();

  ServiceParameters(const std::string& service_type,
                    const std::string& secondary_service_type,
                    const std::string& service_name_prefix,
                    const std::string& service_domain_name,
                    const net::IPAddressNumber& http_ipv4,
                    const net::IPAddressNumber& http_ipv6,
                    uint16 http_port);

  std::string service_type_;
  std::string secondary_service_type_;
  std::string service_name_;
  std::string service_domain_name_;
  net::IPAddressNumber http_ipv4_;
  net::IPAddressNumber http_ipv6_;
  uint16 http_port_;
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_SERVICE_PARAMETERS_H_
