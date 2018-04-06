// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cloud_print_private/cloud_print_private_api.h"

#include <memory>
#include <string>

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/common/extensions/api/cloud_print_private.h"
#include "google_apis/google_api_keys.h"
#include "net/base/network_interfaces.h"
#include "printing/buildflags/buildflags.h"

namespace extensions {

namespace {

const char kErrorIncognito[] = "Cannot access in incognito mode";

CloudPrintTestsDelegate* g_cloud_print_private_api_instance = nullptr;

}  // namespace

CloudPrintTestsDelegate* CloudPrintTestsDelegate::Get() {
  return g_cloud_print_private_api_instance;
}

CloudPrintTestsDelegate::CloudPrintTestsDelegate() {
  g_cloud_print_private_api_instance = this;
}

CloudPrintTestsDelegate::~CloudPrintTestsDelegate() {
  g_cloud_print_private_api_instance = nullptr;
}

CloudPrintPrivateSetupConnectorFunction::
    CloudPrintPrivateSetupConnectorFunction() {
}

CloudPrintPrivateSetupConnectorFunction::
    ~CloudPrintPrivateSetupConnectorFunction() {
}

bool CloudPrintPrivateSetupConnectorFunction::RunAsync() {
  using api::cloud_print_private::SetupConnector::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  if (CloudPrintTestsDelegate::Get()) {
    CloudPrintTestsDelegate::Get()->SetupConnector(
        params->user_email, params->robot_email, params->credentials,
        params->user_settings);
    SendResponse(true);
    return true;
  }

  base::Value user_settings_value =
      base::Value::FromUniquePtrValue(params->user_settings.ToValue());

  CloudPrintProxyService* service =
      CloudPrintProxyServiceFactory::GetForProfile(GetProfile());
  if (!service) {
    error_ = kErrorIncognito;
    return false;
  }
  service->EnableForUserWithRobot(params->credentials, params->robot_email,
                                  params->user_email,
                                  std::move(user_settings_value));
  SendResponse(true);
  return true;
}

CloudPrintPrivateGetHostNameFunction::CloudPrintPrivateGetHostNameFunction() {
}

CloudPrintPrivateGetHostNameFunction::~CloudPrintPrivateGetHostNameFunction() {
}

bool CloudPrintPrivateGetHostNameFunction::RunAsync() {
  SetResult(std::make_unique<base::Value>(
      CloudPrintTestsDelegate::Get()
          ? CloudPrintTestsDelegate::Get()->GetHostName()
          : net::GetHostName()));
  SendResponse(true);
  return true;
}

CloudPrintPrivateGetPrintersFunction::CloudPrintPrivateGetPrintersFunction() {
}

CloudPrintPrivateGetPrintersFunction::~CloudPrintPrivateGetPrintersFunction() {
}

void CloudPrintPrivateGetPrintersFunction::SendResults(
    const std::vector<std::string>& printers) {
  results_ = api::cloud_print_private::GetPrinters::Results::Create(printers);
  SendResponse(true);
}

bool CloudPrintPrivateGetPrintersFunction::RunAsync() {
  if (CloudPrintTestsDelegate::Get()) {
    SendResults(CloudPrintTestsDelegate::Get()->GetPrinters());
    return true;
  }

  CloudPrintProxyService* service =
      CloudPrintProxyServiceFactory::GetForProfile(GetProfile());
  if (!service) {
    error_ = kErrorIncognito;
    return false;
  }
  service->GetPrinters(
      base::Bind(&CloudPrintPrivateGetPrintersFunction::SendResults, this));
  return true;
}


CloudPrintPrivateGetClientIdFunction::CloudPrintPrivateGetClientIdFunction() {
}

CloudPrintPrivateGetClientIdFunction::~CloudPrintPrivateGetClientIdFunction() {
}

bool CloudPrintPrivateGetClientIdFunction::RunAsync() {
  SetResult(std::make_unique<base::Value>(
      CloudPrintTestsDelegate::Get()
          ? CloudPrintTestsDelegate::Get()->GetClientId()
          : google_apis::GetOAuth2ClientID(google_apis::CLIENT_CLOUD_PRINT)));
  SendResponse(true);
  return true;
}

}  // namespace extensions
