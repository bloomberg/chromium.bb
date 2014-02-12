// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/service_parameters.h"

ServiceParameters::ServiceParameters() : http_port_(0) {
}

ServiceParameters::~ServiceParameters() {
}

ServiceParameters::ServiceParameters(const std::string& service_type,
                                     const std::string& secondary_service_type,
                                     const std::string& service_name_prefix,
                                     const std::string& service_domain_name,
                                     const net::IPAddressNumber& http_ipv4,
                                     const net::IPAddressNumber& http_ipv6,
                                     uint16 http_port)
    : service_type_(service_type),
      secondary_service_type_(secondary_service_type),
      service_name_(service_name_prefix + "." + service_type),
      service_domain_name_(service_domain_name),
      http_ipv4_(http_ipv4),
      http_ipv6_(http_ipv6),
      http_port_(http_port) {
}
