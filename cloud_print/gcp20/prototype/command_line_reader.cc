// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/command_line_reader.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "cloud_print/gcp20/prototype/gcp20_switches.h"

namespace command_line_reader {

uint16 ReadHttpPort(uint16 default_value) {
  uint32 http_port = 0;

  std::string http_port_string =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kHttpPort);

  if (!base::StringToUint(http_port_string, &http_port))
    http_port = default_value;

  if (http_port > kuint16max) {
    LOG(ERROR) << "HTTP Port is too large";
    http_port = default_value;
  }

  VLOG(1) << "HTTP port for responses: " << http_port;
  return static_cast<uint16>(http_port);
}

uint32 ReadTtl(uint32 default_value) {
  uint32 ttl = 0;

  if (!base::StringToUint(
          CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kTtl),
          &ttl)) {
    ttl = default_value;
  }

  VLOG(1) << "TTL for announcements: " << ttl;
  return ttl;
}

std::string ReadServiceNamePrefix(const std::string& default_value) {
  std::string service_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kServiceName);

  return service_name.empty() ? default_value : service_name;
}

std::string ReadDomainName(const std::string& default_value) {
  std::string domain_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
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
  std::string filename = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kStatePath);

  if (filename.empty())
    return default_value;
  return filename;
}

}  // namespace command_line_reader

