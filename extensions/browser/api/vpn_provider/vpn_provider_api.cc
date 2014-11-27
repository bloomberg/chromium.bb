// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/vpn_provider/vpn_provider_api.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "extensions/common/api/vpn_provider.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace extensions {

namespace {

namespace api_vpn = extensions::core_api::vpn_provider;

const char kCIDRSeperator[] = "/";

void ConvertParameters(const api_vpn::Parameters& parameters,
                       base::DictionaryValue* parameter_value,
                       std::string* error) {
  std::vector<std::string> cidr_parts;
  if (Tokenize(parameters.address, kCIDRSeperator, &cidr_parts) != 2) {
    *error = "Invalid CIDR address.";
    return;
  }

  parameter_value->SetStringWithoutPathExpansion(
      shill::kAddressParameterThirdPartyVpn, cidr_parts[0]);

  parameter_value->SetStringWithoutPathExpansion(
      shill::kSubnetPrefixParameterThirdPartyVpn, cidr_parts[1]);

  parameter_value->SetStringWithoutPathExpansion(
      shill::kBypassTunnelForIpParameterThirdPartyVpn,
      JoinString(parameters.bypass_tunnel_for_ip, shill::kIPDelimiter));

  if (parameters.mtu) {
    parameter_value->SetStringWithoutPathExpansion(
        shill::kMtuParameterThirdPartyVpn, *parameters.mtu);
  }

  if (parameters.broadcast_address) {
    parameter_value->SetStringWithoutPathExpansion(
        shill::kBroadcastAddressParameterThirdPartyVpn,
        *parameters.broadcast_address);
  }

  if (parameters.domain_search) {
    parameter_value->SetStringWithoutPathExpansion(
        shill::kDomainSearchParameterThirdPartyVpn,
        JoinString(*parameters.domain_search, shill::kNonIPDelimiter));
  }

  parameter_value->SetStringWithoutPathExpansion(
      shill::kDnsServersParameterThirdPartyVpn,
      JoinString(parameters.dns_servers, shill::kIPDelimiter));

  return;
}

}  // namespace

VpnThreadExtensionFunction::~VpnThreadExtensionFunction() {
}

void VpnThreadExtensionFunction::SignalCallCompletionSuccess() {
  Respond(NoArguments());
}

void VpnThreadExtensionFunction::SignalCallCompletionFailure(
    const std::string& error_name,
    const std::string& error_message) {
  if (!error_name.empty() && !error_message.empty()) {
    Respond(Error(error_name + ": " + error_message));
  } else if (!error_name.empty()) {
    Respond(Error(error_name));
  } else {
    Respond(Error(error_message));
  }
}

VpnProviderCreateConfigFunction::~VpnProviderCreateConfigFunction() {
}

ExtensionFunction::ResponseAction VpnProviderCreateConfigFunction::Run() {
  scoped_ptr<api_vpn::CreateConfig::Params> params(
      api_vpn::CreateConfig::Params::Create(*args_));
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnService* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  service->CreateConfiguration(
      extension_id(), extension()->name(), params->name,
      base::Bind(&VpnProviderCreateConfigFunction::SignalCallCompletionSuccess,
                 this),
      base::Bind(&VpnProviderNotifyConnectionStateChangedFunction::
                     SignalCallCompletionFailure,
                 this));

  return RespondLater();
}

VpnProviderDestroyConfigFunction::~VpnProviderDestroyConfigFunction() {
}

ExtensionFunction::ResponseAction VpnProviderDestroyConfigFunction::Run() {
  scoped_ptr<api_vpn::DestroyConfig::Params> params(
      api_vpn::DestroyConfig::Params::Create(*args_));
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnService* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  service->DestroyConfiguration(
      extension_id(), params->name,
      base::Bind(&VpnProviderDestroyConfigFunction::SignalCallCompletionSuccess,
                 this),
      base::Bind(&VpnProviderNotifyConnectionStateChangedFunction::
                     SignalCallCompletionFailure,
                 this));

  return RespondLater();
}

VpnProviderSetParametersFunction::~VpnProviderSetParametersFunction() {
}

ExtensionFunction::ResponseAction VpnProviderSetParametersFunction::Run() {
  scoped_ptr<api_vpn::SetParameters::Params> params(
      api_vpn::SetParameters::Params::Create(*args_));
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnService* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  base::DictionaryValue parameter_value;
  std::string error;
  ConvertParameters(params->parameters, &parameter_value, &error);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  service->SetParameters(
      extension_id(), parameter_value,
      base::Bind(&VpnProviderSetParametersFunction::SignalCallCompletionSuccess,
                 this),
      base::Bind(&VpnProviderNotifyConnectionStateChangedFunction::
                     SignalCallCompletionFailure,
                 this));

  return RespondLater();
}

VpnProviderSendPacketFunction::~VpnProviderSendPacketFunction() {
}

ExtensionFunction::ResponseAction VpnProviderSendPacketFunction::Run() {
  scoped_ptr<api_vpn::SendPacket::Params> params(
      api_vpn::SendPacket::Params::Create(*args_));
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnService* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  service->SendPacket(
      extension_id(), params->data,
      base::Bind(&VpnProviderSendPacketFunction::SignalCallCompletionSuccess,
                 this),
      base::Bind(&VpnProviderNotifyConnectionStateChangedFunction::
                     SignalCallCompletionFailure,
                 this));

  return RespondLater();
}

VpnProviderNotifyConnectionStateChangedFunction::
    ~VpnProviderNotifyConnectionStateChangedFunction() {
}

ExtensionFunction::ResponseAction
VpnProviderNotifyConnectionStateChangedFunction::Run() {
  scoped_ptr<api_vpn::NotifyConnectionStateChanged::Params> params(
      api_vpn::NotifyConnectionStateChanged::Params::Create(*args_));
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnService* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  service->NotifyConnectionStateChanged(
      extension_id(), params->state,
      base::Bind(&VpnProviderNotifyConnectionStateChangedFunction::
                     SignalCallCompletionSuccess,
                 this),
      base::Bind(&VpnProviderNotifyConnectionStateChangedFunction::
                     SignalCallCompletionFailure,
                 this));

  return RespondLater();
}

}  // namespace extensions
