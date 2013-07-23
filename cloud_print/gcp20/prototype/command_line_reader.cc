// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/command_line_reader.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace {

const uint16 kDefaultHttpPort = 10101;
const uint32 kDefaultTTL = 60*60;

}  // namespace

namespace command_line_reader {

uint16 ReadHttpPort() {
  uint32 http_port = kDefaultHttpPort;

  std::string http_port_string =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("http-port");

  if (!base::StringToUint(http_port_string, &http_port))
    http_port = kDefaultHttpPort;

  if (http_port > kuint16max) {
    LOG(ERROR) << "Port " << http_port << " is too large (maximum is " <<
        kuint16max << "). Using default port: " << kDefaultHttpPort;

    http_port = kDefaultHttpPort;
  }

  VLOG(1) << "HTTP port for responses: " << http_port;
  return static_cast<uint16>(http_port);
}

uint32 ReadTtl() {
  uint32 ttl = kDefaultTTL;

  if (!base::StringToUint(
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("ttl"), &ttl)) {
    ttl = kDefaultTTL;
  }

  VLOG(1) << "TTL for announcements: " << ttl;
  return ttl;
}

}  // namespace command_line_reader

