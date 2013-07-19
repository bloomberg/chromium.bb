// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/printer.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "cloud_print/gcp20/prototype/service_parameters.h"
#include "net/base/net_util.h"
#include "net/base/url_util.h"

namespace {

const char* kServiceType = "_privet._tcp.local";
const char* kServiceNamePrefix = "first_gcp20_device";
const char* kServiceDomainName = "my-privet-device.local";

const char* kPrinterName = "Google GCP2.0 Prototype";

const uint16 kDefaultTTL = 60*60;
const uint16 kDefaultHttpPort = 10101;

const char* kCdd =
"{\n"
" 'version': '1.0',\n"
"  'printer': {\n"
"    'vendor_capability': [\n"
"      {\n"
"        'id': 'psk:MediaType',\n"
"        'display_name': 'Media Type',\n"
"        'type': 'SELECT',\n"
"        'select_cap': {\n"
"          'option': [\n"
"            {\n"
"              'value': 'psk:Plain',\n"
"              'display_name': 'Plain Paper',\n"
"              'is_default': true\n"
"            },\n"
"            {\n"
"              'value': 'ns0000:Glossy',\n"
"              'display_name': 'Glossy Photo',\n"
"              'is_default': false\n"
"            }\n"
"          ]\n"
"        }\n"
"      }\n"
"    ],\n"
"    'reverse_order': { 'default': false }\n"
"  }\n"
"}\n";

std::string GenerateProxyId() {
  return "{" + base::GenerateGUID() + "}";
}

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

Printer::RegistrationInfo::RegistrationInfo() : state(DEV_REG_UNREGISTERED) {
}

Printer::RegistrationInfo::~RegistrationInfo() {
}

Printer::Printer() : http_server_(this) {
}

Printer::~Printer() {
  Stop();
}

bool Printer::Start() {
  if (IsOnline())
    return true;

  // TODO(maksymb): Add switch for command line to control interface name.
  net::IPAddressNumber ip = GetLocalIp("", false);
  if (ip.empty()) {
    LOG(ERROR) << "No local IP found. Cannot start printer.";
    return false;
  }
  VLOG(1) << "Local address: " << net::IPAddressToString(ip);

  uint16 port = ReadHttpPortFromCommandLine();

  // TODO(maksymb): Add loading state from drive.
  reg_info_ = RegistrationInfo();

  // Starting DNS-SD server.
  bool success = dns_server_.Start(
      ServiceParameters(kServiceType, kServiceNamePrefix, kServiceDomainName,
                        ip, port),
      ReadTtlFromCommandLine(),
      CreateTxt());

  if (!success)
    return false;

  // Starting HTTP server.
  success = http_server_.Start(port);
  if (!success)
    return false;
  // TODO(maksymb): Check what happened if DNS server will start but HTTP
  // server won't.

  // Creating Cloud Requester.
  requester_.reset(
      new CloudPrintRequester(
          base::MessageLoop::current()->message_loop_proxy(),
          this));

  return true;
}

bool Printer::IsOnline() const {
  return requester_;
}

void Printer::Stop() {
  dns_server_.Shutdown();
  http_server_.Shutdown();
  requester_.reset();
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationStart(
    const std::string& user) {
  PrivetHttpServer::RegistrationErrorStatus status;
  if (!CheckRegistrationState(user, false, &status))
    return status;

  if (reg_info_.state != RegistrationInfo::DEV_REG_UNREGISTERED)
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;

  reg_info_ = RegistrationInfo();
  reg_info_.user = user;
  reg_info_.state = RegistrationInfo::DEV_REG_REGISTRATION_STARTED;

  requester_->StartRegistration(GenerateProxyId(), kPrinterName, user, kCdd);

  return PrivetHttpServer::REG_ERROR_OK;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationGetClaimToken(
    const std::string& user,
    std::string* token,
    std::string* claim_url) {
  PrivetHttpServer::RegistrationErrorStatus status;
  if (!CheckRegistrationState(user, false, &status))
    return status;

  // TODO(maksymb): Add user confirmation.

  if (reg_info_.state == RegistrationInfo::DEV_REG_REGISTRATION_STARTED)
    return PrivetHttpServer::REG_ERROR_DEVICE_BUSY;

  if (reg_info_.state ==
      RegistrationInfo::DEV_REG_REGISTRATION_CLAIM_TOKEN_READY) {
    *token = reg_info_.registration_token;
    *claim_url = reg_info_.complete_invite_url;
    return PrivetHttpServer::REG_ERROR_OK;
  }

  return PrivetHttpServer::REG_ERROR_INVALID_ACTION;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationComplete(
    const std::string& user,
    std::string* device_id) {
  PrivetHttpServer::RegistrationErrorStatus status;
  if (!CheckRegistrationState(user, false, &status))
    return status;

  if (reg_info_.state !=
      RegistrationInfo::DEV_REG_REGISTRATION_CLAIM_TOKEN_READY)
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;

  reg_info_.state = RegistrationInfo::DEV_REG_REGISTRATION_COMPLETING;
  requester_->CompleteRegistration();

  *device_id = reg_info_.device_id;

  return PrivetHttpServer::REG_ERROR_OK;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationCancel(
    const std::string& user) {
  PrivetHttpServer::RegistrationErrorStatus status;
  if (!CheckRegistrationState(user, true, &status))
    return status;

  if (reg_info_.state == RegistrationInfo::DEV_REG_UNREGISTERED)
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;

  reg_info_ = RegistrationInfo();
  return PrivetHttpServer::REG_ERROR_OK;
}

void Printer::GetRegistrationServerError(std::string* description) {
  DCHECK_EQ(reg_info_.state, RegistrationInfo::DEV_REG_REGISTRATION_ERROR) <<
      "Method shouldn't be called when not needed.";

  *description = reg_info_.error_description;
}

void Printer::CreateInfo(PrivetHttpServer::DeviceInfo* info) {
  *info = PrivetHttpServer::DeviceInfo();
  info->version = "1.0";
  info->manufacturer = "Google";
  if (reg_info_.state == RegistrationInfo::DEV_REG_UNREGISTERED)
    info->api.push_back("/privet/register");
}

void Printer::OnRegistrationStartResponseParsed(
    const std::string& registration_token,
    const std::string& complete_invite_url,
    const std::string& device_id) {
  reg_info_.state = RegistrationInfo::DEV_REG_REGISTRATION_CLAIM_TOKEN_READY;
  reg_info_.device_id = device_id;
  reg_info_.registration_token = registration_token;
  reg_info_.complete_invite_url = complete_invite_url;
}

void Printer::OnGetAuthCodeResponseParsed(const std::string& refresh_token) {
  reg_info_.state = RegistrationInfo::DEV_REG_REGISTERED;
  reg_info_.refresh_token = refresh_token;
}

void Printer::OnRegistrationError(const std::string& description) {
  LOG(ERROR) << "server_error: " << description;

  // TODO(maksymb): Implement waiting after error and timeout of registration.
  reg_info_.state = RegistrationInfo::DEV_REG_REGISTRATION_ERROR;
  reg_info_.error_description = description;
}

bool Printer::CheckRegistrationState(
    const std::string& user,
    bool ignore_error,
    PrivetHttpServer::RegistrationErrorStatus* status) const {
  if (reg_info_.state == RegistrationInfo::DEV_REG_REGISTERED) {
     *status = PrivetHttpServer::REG_ERROR_REGISTERED;
    return false;
  }

  if (reg_info_.state != RegistrationInfo::DEV_REG_UNREGISTERED &&
      user != reg_info_.user) {
    *status = PrivetHttpServer::REG_ERROR_DEVICE_BUSY;
    return false;
  }

  if (!ignore_error &&
      reg_info_.state == RegistrationInfo::DEV_REG_REGISTRATION_ERROR) {
    *status = PrivetHttpServer::REG_ERROR_SERVER_ERROR;
    return false;
  }

  return true;
}

std::vector<std::string> Printer::CreateTxt() const {
  std::vector<std::string> txt;
  txt.push_back("txtvers=1");
  txt.push_back("ty=" + std::string(kPrinterName));
  txt.push_back("note=");
  txt.push_back("url=https://www.google.com/cloudprint");
  txt.push_back("type=printer");
  txt.push_back("id=" + reg_info_.device_id);
  txt.push_back("cs=offline");

  return txt;
}

