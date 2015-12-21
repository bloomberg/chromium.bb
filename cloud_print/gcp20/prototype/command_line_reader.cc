// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/command_line_reader.h"

#include <stdint.h>

#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "cloud_print/gcp20/prototype/gcp20_switches.h"

namespace command_line_reader {

uint16_t ReadHttpPort(uint16_t default_value) {
  uint32_t http_port = 0;

  std::string http_port_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kHttpPort);

  if (!base::StringToUint(http_port_string, &http_port))
    http_port = default_value;

  if (http_port > std::numeric_limits<uint16_t>::max()) {
    LOG(ERROR) << "HTTP Port is too large";
    http_port = default_value;
  }

  VLOG(1) << "HTTP port for responses: " << http_port;
  return static_cast<uint16_t>(http_port);
}

uint32_t ReadTtl(uint32_t default_value) {
  uint32_t ttl = 0;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!base::StringToUint(
          command_line->GetSwitchValueASCII(switches::kTtl),
          &ttl)) {
    ttl = default_value;
  }

  VLOG(1) << "TTL for announcements: " << ttl;
  return ttl;
}

std::string ReadServiceNamePrefix(const std::string& default_value) {
  std::string service_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kServiceName);

  return service_name.empty() ? default_value : service_name;
}

std::string ReadDomainName(const std::string& default_value) {
  std::string domain_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDomainName);

  if (domain_name.empty())
    return default_value;

  std::string suffix = ".local";
  if (domain_name == suffix) {
    LOG(ERROR) << "Domain name cannot be only \"" << suffix << "\"";
    return default_value;
  }

  if (domain_name.size() < suffix.size() ||
      domain_name.substr(domain_name.size() - suffix.size()) != suffix) {
    LOG(ERROR) << "Domain name should end with \"" << suffix << "\"";
    return default_value;
  }

  return domain_name;
}

std::string ReadStatePath(const std::string& default_value) {
  std::string filename =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kStatePath);

  if (filename.empty())
    return default_value;
  return filename;
}

}  // namespace command_line_reader

