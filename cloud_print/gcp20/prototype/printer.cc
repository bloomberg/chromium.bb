// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/printer.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "cloud_print/gcp20/prototype/service_parameters.h"
#include "net/base/net_util.h"

namespace {

const char* kServiceType = "_privet._tcp.local";
const char* kServiceNamePrefix = "first_gcp20_device";
const char* kServiceDomainName = "my-privet-device.local";

const uint16 kDefaultTTL = 60*60;
const uint16 kDefaultHttpPort = 10101;

uint16 ReadHttpPortFromCommandLine() {
  uint32 http_port_tmp = kDefaultHttpPort;

  std::string http_port_string_tmp =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("http-port");
  base::StringToUint(http_port_string_tmp, &http_port_tmp);

  if (http_port_tmp > kuint16max) {
    LOG(ERROR) << "Port " << http_port_tmp << " is too large (maximum is " <<
        kDefaultHttpPort << "). Using default port.";

    http_port_tmp = kDefaultHttpPort;
  }

  VLOG(1) << "HTTP port for responses: " << http_port_tmp;
  return static_cast<uint16>(http_port_tmp);
}

uint16 ReadTtlFromCommandLine() {
  uint32 ttl = kDefaultTTL;

  base::StringToUint(
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("ttl"), &ttl);

  VLOG(1) << "TTL for announcements: " << ttl;
  return ttl;
}

// Returns local IP address number of first interface found (except loopback).
// Return value is empty if no interface found. Possible interfaces names are
// "eth0", "wlan0" etc. If interface name is empty, function will return IP
// address of first interface found.
net::IPAddressNumber GetLocalIp(const std::string& interface_name,
                                bool return_ipv6_number) {
  net::NetworkInterfaceList interfaces;
  bool success = net::GetNetworkList(&interfaces);
  DCHECK(success);

  size_t expected_address_size = return_ipv6_number ? net::kIPv6AddressSize
                                                    : net::kIPv4AddressSize;

  for (net::NetworkInterfaceList::iterator iter = interfaces.begin();
       iter != interfaces.end(); ++iter) {
    if (iter->address.size() == expected_address_size &&
        (interface_name.empty() || interface_name == iter->name)) {
      LOG(INFO) << net::IPAddressToString(iter->address);
      return iter->address;
    }
  }

  return net::IPAddressNumber();
}

}  // namespace

Printer::Printer() : initialized_(false) {
}

Printer::~Printer() {
  Stop();
}

bool Printer::Start() {
  if (initialized_)
    return true;

  // TODO(maksymb): Add switch for command line to control interface name.
  net::IPAddressNumber ip = GetLocalIp("", false);
  if (ip.empty()) {
    LOG(ERROR) << "No local IP found. Cannot start printer.";
    return false;
  }
  VLOG(1) << "Local address: " << net::IPAddressToString(ip);

  // Starting DNS-SD server.
  initialized_ = dns_server_.Start(
      ServiceParameters(kServiceType,
                        kServiceNamePrefix,
                        kServiceDomainName,
                        ip,
                        ReadHttpPortFromCommandLine()),
      ReadTtlFromCommandLine(),
      CreateTxt());

  return initialized_;
}

void Printer::Stop() {
  if (!initialized_)
    return;

  dns_server_.Shutdown();
}

std::vector<std::string> Printer::CreateTxt() const {
  std::vector<std::string> txt;

  txt.push_back("txtvers=1");
  txt.push_back("ty=Google Cloud Ready Printer GCP2.0 Prototype");
  txt.push_back("note=Virtual printer");
  txt.push_back("url=https://www.google.com/cloudprint");
  txt.push_back("type=printer");
  txt.push_back("id=");
  txt.push_back("cs=offline");

  return txt;
}

