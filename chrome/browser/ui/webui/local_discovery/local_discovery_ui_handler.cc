// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"

#include <set>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "chrome/browser/local_discovery/privet_http_impl.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_base.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/page_transition_types.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"
#include "net/http/http_status_code.h"

namespace local_discovery {

namespace {
const char kPrivetAutomatedClaimURLFormat[] = "%s/confirm?token=%s";
const int kRegistrationAnnouncementTimeoutSeconds = 5;

LocalDiscoveryUIHandler::Factory* g_factory = NULL;
int g_num_visible = 0;

}  // namespace

LocalDiscoveryUIHandler::LocalDiscoveryUIHandler() : is_visible_(false) {
}

LocalDiscoveryUIHandler::LocalDiscoveryUIHandler(
    scoped_ptr<PrivetDeviceLister> privet_lister) : is_visible_(false) {
  privet_lister.swap(privet_lister_);
}

LocalDiscoveryUIHandler::~LocalDiscoveryUIHandler() {
  ResetCurrentRegistration();
  SetIsVisible(false);
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

// static
bool LocalDiscoveryUIHandler::GetHasVisible() {
  return g_num_visible != 0;
}

void LocalDiscoveryUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("start", base::Bind(
      &LocalDiscoveryUIHandler::HandleStart,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("isVisible", base::Bind(
      &LocalDiscoveryUIHandler::HandleIsVisible,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("registerDevice", base::Bind(
      &LocalDiscoveryUIHandler::HandleRegisterDevice,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("cancelRegistration", base::Bind(
      &LocalDiscoveryUIHandler::HandleCancelRegistration,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestPrinterList", base::Bind(
      &LocalDiscoveryUIHandler::HandleRequestPrinterList,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openCloudPrintURL", base::Bind(
      &LocalDiscoveryUIHandler::HandleOpenCloudPrintURL,
      base::Unretained(this)));
}

void LocalDiscoveryUIHandler::HandleStart(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());

  // If privet_lister_ is already set, it is a mock used for tests or the result
  // of a reload.
  if (!privet_lister_) {
    service_discovery_client_ = ServiceDiscoveryHostClientFactory::GetClient();
    privet_lister_.reset(new PrivetDeviceListerImpl(
        service_discovery_client_.get(), this));
    privet_http_factory_.reset(new PrivetHTTPAsynchronousFactoryImpl(
        service_discovery_client_.get(),
        profile->GetRequestContext()));
  }

  privet_lister_->Start();
  privet_lister_->DiscoverNewDevices(false);
}

void LocalDiscoveryUIHandler::HandleIsVisible(const base::ListValue* args) {
  bool is_visible = false;
  bool rv = args->GetBoolean(0, &is_visible);
  DCHECK(rv);
  SetIsVisible(is_visible);
}

void LocalDiscoveryUIHandler::HandleRegisterDevice(
    const base::ListValue* args) {
  std::string device;

  bool rv = args->GetString(0, &device);
  DCHECK(rv);

  privet_resolution_ = privet_http_factory_->CreatePrivetHTTP(
      device,
      device_descriptions_[device].address,
      base::Bind(&LocalDiscoveryUIHandler::StartRegisterHTTP,
                 base::Unretained(this)));
  privet_resolution_->Start();
}

void LocalDiscoveryUIHandler::HandleCancelRegistration(
    const base::ListValue* args) {
  ResetCurrentRegistration();
}

void LocalDiscoveryUIHandler::HandleRequestPrinterList(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);

  cloud_print_printer_list_.reset(new CloudPrintPrinterList(
      profile->GetRequestContext(),
      GetCloudPrintBaseUrl(),
      token_service,
      this));
  cloud_print_printer_list_->Start();
}

void LocalDiscoveryUIHandler::HandleOpenCloudPrintURL(
    const base::ListValue* args) {
  std::string url;
  bool rv = args->GetString(0, &url);
  DCHECK(rv);

  GURL url_full(GetCloudPrintBaseUrl() + url);

  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  DCHECK(browser);

  chrome::AddSelectedTabWithURL(browser,
                                url_full,
                                content::PAGE_TRANSITION_FROM_API);
}

void LocalDiscoveryUIHandler::StartRegisterHTTP(
    scoped_ptr<PrivetHTTPClient> http_client) {
  current_http_client_.swap(http_client);

  std::string user = GetSyncAccount();

  if (!current_http_client_) {
    SendRegisterError();
    return;
  }

  current_register_operation_ =
      current_http_client_->CreateRegisterOperation(user, this);
  current_register_operation_->Start();
}

void LocalDiscoveryUIHandler::OnPrivetRegisterClaimToken(
    PrivetRegisterOperation* operation,
    const std::string& token,
    const GURL& url) {
  web_ui()->CallJavascriptFunction(
      "local_discovery.onRegistrationConfirmedOnPrinter");
  if (device_descriptions_.count(current_http_client_->GetName()) == 0) {
    SendRegisterError();
    return;
  }

  std::string base_url = GetCloudPrintBaseUrl();

  GURL automated_claim_url(base::StringPrintf(
      kPrivetAutomatedClaimURLFormat,
      base_url.c_str(),
      token.c_str()));

  Profile* profile = Profile::FromWebUI(web_ui());

  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);

  if (!token_service) {
    SendRegisterError();
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
    PrivetRegisterOperation* operation,
    const std::string& action,
    PrivetRegisterOperation::FailureReason reason,
    int printer_http_code,
    const DictionaryValue* json) {
  // TODO(noamsml): Add detailed error message.
  SendRegisterError();
}

void LocalDiscoveryUIHandler::OnPrivetRegisterDone(
    PrivetRegisterOperation* operation,
    const std::string& device_id) {
  std::string name = operation->GetHTTPClient()->GetName();

  current_register_operation_.reset();
  current_http_client_.reset();

  DeviceDescriptionMap::iterator found = device_descriptions_.find(name);

  if (found == device_descriptions_.end() || found->second.id.empty()) {
    new_register_device_ = name;
    registration_announce_timeout_.Reset(base::Bind(
        &LocalDiscoveryUIHandler::OnAnnouncementTimeoutReached,
        base::Unretained(this)));

    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        registration_announce_timeout_.callback(),
        base::TimeDelta::FromSeconds(kRegistrationAnnouncementTimeoutSeconds));
  }
}

void LocalDiscoveryUIHandler::OnAnnouncementTimeoutReached() {
  new_register_device_.clear();
  registration_announce_timeout_.Cancel();

  SendRegisterError();
}

void LocalDiscoveryUIHandler::OnConfirmDone(
    CloudPrintBaseApiFlow::Status status) {
  if (status == CloudPrintBaseApiFlow::SUCCESS) {
    confirm_api_call_flow_.reset();
    current_register_operation_->CompleteRegistration();
  } else {
    // TODO(noamsml): Add detailed error message.
    SendRegisterError();
  }
}

void LocalDiscoveryUIHandler::DeviceChanged(
    bool added,
    const std::string& name,
    const DeviceDescription& description) {
  device_descriptions_[name] = description;

  base::DictionaryValue info;

  base::StringValue service_name(name);
  scoped_ptr<base::Value> null_value(base::Value::CreateNullValue());

  if (description.id.empty()) {
    info.SetString("service_name", name);
    info.SetString("human_readable_name", description.name);
    info.SetString("description", description.description);

    web_ui()->CallJavascriptFunction(
        "local_discovery.onUnregisteredDeviceUpdate",
        service_name, info);
  } else {
    web_ui()->CallJavascriptFunction(
        "local_discovery.onUnregisteredDeviceUpdate",
        service_name, *null_value);

    if (name == new_register_device_) {
      new_register_device_.clear();
      SendRegisterDone();
    }
  }
}

void LocalDiscoveryUIHandler::DeviceRemoved(const std::string& name) {
  device_descriptions_.erase(name);
  scoped_ptr<base::Value> null_value(base::Value::CreateNullValue());
  base::StringValue name_value(name);

  web_ui()->CallJavascriptFunction("local_discovery.onUnregisteredDeviceUpdate",
                                   name_value, *null_value);
}

void LocalDiscoveryUIHandler::DeviceCacheFlushed() {
  web_ui()->CallJavascriptFunction("local_discovery.onDeviceCacheFlushed");
  privet_lister_->DiscoverNewDevices(false);
}

void LocalDiscoveryUIHandler::OnCloudPrintPrinterListReady() {
  base::ListValue printer_object_list;
  std::set<std::string> local_ids;

  for (DeviceDescriptionMap::iterator i = device_descriptions_.begin();
       i != device_descriptions_.end();
       i++) {
    std::string device_id = i->second.id;
    if (!device_id.empty()) {
      const CloudPrintPrinterList::PrinterDetails* details =
          cloud_print_printer_list_->GetDetailsFor(device_id);

      if (details) {
        local_ids.insert(device_id);
        printer_object_list.Append(CreatePrinterInfo(*details).release());
      }
    }
  }

  for (CloudPrintPrinterList::iterator i = cloud_print_printer_list_->begin();
       i != cloud_print_printer_list_->end(); i++) {
    if (local_ids.count(i->id) == 0) {
      printer_object_list.Append(CreatePrinterInfo(*i).release());
    }
  }

  web_ui()->CallJavascriptFunction(
      "local_discovery.onCloudDeviceListAvailable", printer_object_list);
}

void LocalDiscoveryUIHandler::OnCloudPrintPrinterListUnavailable() {
}


void LocalDiscoveryUIHandler::SendRegisterError() {
  web_ui()->CallJavascriptFunction("local_discovery.onRegistrationFailed");
}

void LocalDiscoveryUIHandler::SendRegisterDone() {
  web_ui()->CallJavascriptFunction("local_discovery.onRegistrationSuccess");
}

void LocalDiscoveryUIHandler::SetIsVisible(bool visible) {
  if (visible != is_visible_) {
    g_num_visible += visible ? 1 : -1;
    is_visible_ = visible;
  }
}

std::string LocalDiscoveryUIHandler::GetSyncAccount() {
  Profile* profile = Profile::FromWebUI(web_ui());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile);

  if (!signin_manager) {
    return "";
  }

  return signin_manager->GetAuthenticatedUsername();
}

std::string LocalDiscoveryUIHandler::GetCloudPrintBaseUrl() {
  CloudPrintURL cloud_print_url(Profile::FromWebUI(web_ui()));

  return cloud_print_url.GetCloudPrintServiceURL().spec();
}

// TODO(noamsml): Create master object for registration flow.
void LocalDiscoveryUIHandler::ResetCurrentRegistration() {
  if (current_register_operation_.get()) {
    current_register_operation_->Cancel();
    current_register_operation_.reset();
  }

  confirm_api_call_flow_.reset();
  privet_resolution_.reset();
  current_http_client_.reset();
}

scoped_ptr<base::DictionaryValue> LocalDiscoveryUIHandler::CreatePrinterInfo(
    const CloudPrintPrinterList::PrinterDetails& description) {
  scoped_ptr<base::DictionaryValue> return_value(new base::DictionaryValue);

  return_value->SetString("id", description.id);
  return_value->SetString("display_name", description.display_name);
  return_value->SetString("description", description.description);

  return return_value.Pass();
}

}  // namespace local_discovery
