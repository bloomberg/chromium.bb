// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "chrome/browser/local_discovery/privet_http_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_base.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "content/public/browser/web_ui.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"
#include "net/http/http_status_code.h"

namespace local_discovery {

namespace {
// TODO(noamsml): This is a temporary shim until automated_url is in the
// response.
const char kPrivetAutomatedClaimURLFormat[] = "%s/confirm?token=%s";

std::string IPAddressToHostString(const net::IPAddressNumber& address) {
  std::string address_str = net::IPAddressToString(address);

  // IPv6 addresses need to be surrounded by brackets.
  if (address.size() == net::kIPv6AddressSize) {
    address_str = base::StringPrintf("[%s]", address_str.c_str());
  }

  return address_str;
}

LocalDiscoveryUIHandler::Factory* g_factory = NULL;
}  // namespace

LocalDiscoveryUIHandler::LocalDiscoveryUIHandler() {
}

LocalDiscoveryUIHandler::LocalDiscoveryUIHandler(
    scoped_ptr<PrivetDeviceLister> privet_lister) {
  privet_lister.swap(privet_lister_);
}

LocalDiscoveryUIHandler::~LocalDiscoveryUIHandler() {
  if (service_discovery_client_.get()) {
    service_discovery_client_ = NULL;
    ServiceDiscoveryHostClientFactory::ReleaseClient();
  }
}

// static
LocalDiscoveryUIHandler* LocalDiscoveryUIHandler::Create() {
  if (g_factory) return g_factory->CreateLocalDiscoveryUIHandler();
  return new LocalDiscoveryUIHandler();
}

// static
void LocalDiscoveryUIHandler::SetFactory(Factory* factory) {
  g_factory = factory;
}

void LocalDiscoveryUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("start", base::Bind(
      &LocalDiscoveryUIHandler::HandleStart,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("registerDevice", base::Bind(
      &LocalDiscoveryUIHandler::HandleRegisterDevice,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("info", base::Bind(
      &LocalDiscoveryUIHandler::HandleInfoRequested,
      base::Unretained(this)));
}

void LocalDiscoveryUIHandler::HandleStart(const base::ListValue* args) {
  // If privet_lister_ is already set, it is a mock used for tests or the result
  // of a reload.
  if (!privet_lister_) {
    service_discovery_client_ = ServiceDiscoveryHostClientFactory::GetClient();
    privet_lister_.reset(new PrivetDeviceListerImpl(
        service_discovery_client_.get(), this));
  }

  privet_lister_->Start();
  privet_lister_->DiscoverNewDevices(false);
}

void LocalDiscoveryUIHandler::HandleRegisterDevice(
    const base::ListValue* args) {
  std::string device_name;

  bool rv = args->GetString(0, &device_name);
  DCHECK(rv);
  current_http_device_ = device_name;

  domain_resolver_ = service_discovery_client_->CreateLocalDomainResolver(
      device_descriptions_[device_name].address.host(),
      net::ADDRESS_FAMILY_UNSPECIFIED,
      base::Bind(&LocalDiscoveryUIHandler::StartRegisterHTTP,
                 base::Unretained(this)));

    domain_resolver_->Start();
}

void LocalDiscoveryUIHandler::HandleInfoRequested(const base::ListValue* args) {
  std::string device_name;
  args->GetString(0, &device_name);
  current_http_device_ = device_name;

  domain_resolver_ = service_discovery_client_->CreateLocalDomainResolver(
      device_descriptions_[device_name].address.host(),
      net::ADDRESS_FAMILY_UNSPECIFIED,
      base::Bind(&LocalDiscoveryUIHandler::StartInfoHTTP,
                 base::Unretained(this)));

  domain_resolver_->Start();
}

void LocalDiscoveryUIHandler::StartRegisterHTTP(bool success,
    const net::IPAddressNumber& address) {
  if (!success) {
    LogRegisterErrorToWeb("Resolution failed");
    return;
  }

  if (device_descriptions_.count(current_http_device_) == 0) {
    LogRegisterErrorToWeb("Device no longer exists");
    return;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile);

  if (!signin_manager) {
    LogRegisterErrorToWeb("You must be signed in");
    return;
  }

  std::string username = signin_manager->GetAuthenticatedUsername();

  std::string address_str = IPAddressToHostString(address);
  int port = device_descriptions_[current_http_device_].address.port();

  current_http_client_.reset(new PrivetHTTPClientImpl(
      net::HostPortPair(address_str, port),
      Profile::FromWebUI(web_ui())->GetRequestContext()));
  current_register_operation_ =
      current_http_client_->CreateRegisterOperation(username, this);
  current_register_operation_->Start();
}

void LocalDiscoveryUIHandler::StartInfoHTTP(bool success,
    const net::IPAddressNumber& address) {
  if (!success) {
    LogInfoErrorToWeb("Resolution failed");
    return;
  }

  if (device_descriptions_.count(current_http_device_) == 0) {
    LogRegisterErrorToWeb("Device no longer exists");
    return;
  }

  std::string address_str = IPAddressToHostString(address);

  int port = device_descriptions_[current_http_device_].address.port();

  current_http_client_.reset(new PrivetHTTPClientImpl(
      net::HostPortPair(address_str, port),
      Profile::FromWebUI(web_ui())->GetRequestContext()));
  current_info_operation_ = current_http_client_->CreateInfoOperation(this);
  current_info_operation_->Start();
}

void LocalDiscoveryUIHandler::OnPrivetRegisterClaimToken(
    const std::string& token,
    const GURL& url) {
  if (device_descriptions_.count(current_http_device_) == 0) {
    LogRegisterErrorToWeb("Device no longer exists");
    return;
  }

  GURL automated_claim_url(base::StringPrintf(
      kPrivetAutomatedClaimURLFormat,
      device_descriptions_[current_http_device_].url.c_str(),
      token.c_str()));

  Profile* profile = Profile::FromWebUI(web_ui());

  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);

  if (!token_service) {
    LogRegisterErrorToWeb("Could not get token service");
    return;
  }

  confirm_api_call_flow_.reset(new PrivetConfirmApiCallFlow(
      profile->GetRequestContext(),
      token_service,
      automated_claim_url,
      base::Bind(&LocalDiscoveryUIHandler::OnConfirmDone,
                 base::Unretained(this))));

  confirm_api_call_flow_->Start();
}

void LocalDiscoveryUIHandler::OnPrivetRegisterError(
    const std::string& action,
    PrivetRegisterOperation::FailureReason reason,
    int printer_http_code,
    const DictionaryValue* json) {
  // TODO(noamsml): Add detailed error message.
  LogRegisterErrorToWeb("Registration error");
}

void LocalDiscoveryUIHandler::OnPrivetRegisterDone(
    const std::string& device_id) {
  current_register_operation_.reset();
  current_http_client_.reset();

  LogRegisterDoneToWeb(device_id);
}

void LocalDiscoveryUIHandler::OnConfirmDone(
    PrivetConfirmApiCallFlow::Status status) {
  if (status == PrivetConfirmApiCallFlow::SUCCESS) {
    DLOG(INFO) << "Confirm success.";
    confirm_api_call_flow_.reset();
    current_register_operation_->CompleteRegistration();
  } else {
    // TODO(noamsml): Add detailed error message.
    LogRegisterErrorToWeb("Confirm error");
  }
}

void LocalDiscoveryUIHandler::DeviceChanged(
    bool added,
    const std::string& name,
    const DeviceDescription& description) {
  device_descriptions_[name] = description;

  base::StringValue service_name(name);
  base::DictionaryValue info;
  info.SetString("domain", description.address.host());
  info.SetInteger("port", description.address.port());
  std::string ip_addr_string;
  if (!description.ip_address.empty())
    ip_addr_string = net::IPAddressToString(description.ip_address);

  info.SetString("ip", ip_addr_string);
  info.SetString("lastSeen", "unknown");
  info.SetBoolean("registered", !description.id.empty());

  web_ui()->CallJavascriptFunction("local_discovery.onServiceUpdate",
                                   service_name, info);
}

void LocalDiscoveryUIHandler::DeviceRemoved(const std::string& name) {
  device_descriptions_.erase(name);
  scoped_ptr<base::Value> null_value(base::Value::CreateNullValue());
  base::StringValue name_value(name);

  web_ui()->CallJavascriptFunction("local_discovery.onServiceUpdate",
                                   name_value, *null_value);
}

void LocalDiscoveryUIHandler::LogRegisterErrorToWeb(const std::string& error) {
  base::StringValue error_value(error);
  web_ui()->CallJavascriptFunction("local_discovery.registrationFailed",
                                   error_value);
  DLOG(ERROR) << error;
}

void LocalDiscoveryUIHandler::LogRegisterDoneToWeb(const std::string& id) {
  base::StringValue id_value(id);
  web_ui()->CallJavascriptFunction("local_discovery.registrationSuccess",
                                   id_value);
  DLOG(INFO) << "Registered " << id;
}

void LocalDiscoveryUIHandler::LogInfoErrorToWeb(const std::string& error) {
  base::StringValue error_value(error);
  web_ui()->CallJavascriptFunction("local_discovery.infoFailed", error_value);
  LOG(ERROR) << error;
}

void LocalDiscoveryUIHandler::OnPrivetInfoDone(
    int http_code,
    const base::DictionaryValue* json_value) {
  if (http_code != net::HTTP_OK || !json_value) {
    LogInfoErrorToWeb(base::StringPrintf("HTTP error %d", http_code));
    return;
  }

  web_ui()->CallJavascriptFunction("local_discovery.renderInfo", *json_value);
}

}  // namespace local_discovery
