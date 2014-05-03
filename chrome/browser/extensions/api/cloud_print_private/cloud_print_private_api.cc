// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cloud_print_private/cloud_print_private_api.h"

#include <string>

#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/common/extensions/api/cloud_print_private.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_util.h"

namespace extensions {

CloudPrintTestsDelegate* CloudPrintTestsDelegate::instance_ = NULL;

CloudPrintTestsDelegate* CloudPrintTestsDelegate::instance() {
  return instance_;
}

CloudPrintTestsDelegate::CloudPrintTestsDelegate() {
  instance_ = this;
}

CloudPrintTestsDelegate::~CloudPrintTestsDelegate() {
  instance_ = NULL;
}

CloudPrintPrivateSetupConnectorFunction::
    CloudPrintPrivateSetupConnectorFunction() {
}

CloudPrintPrivateSetupConnectorFunction::
    ~CloudPrintPrivateSetupConnectorFunction() {
}

bool CloudPrintPrivateSetupConnectorFunction::RunAsync() {
#if defined(ENABLE_FULL_PRINTING)
  using api::cloud_print_private::SetupConnector::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  if (CloudPrintTestsDelegate::instance()) {
    CloudPrintTestsDelegate::instance()->SetupConnector(
        params->user_email,
        params->robot_email,
        params->credentials,
        params->user_settings);
  } else {
    CloudPrintProxyService* service =
        CloudPrintProxyServiceFactory::GetForProfile(GetProfile());
    if (!service)
      return false;
    scoped_ptr<base::DictionaryValue> user_setings(
        params->user_settings.ToValue());
    service->EnableForUserWithRobot(params->credentials,
                                    params->robot_email,
                                    params->user_email,
                                    *user_setings);
  }
  SendResponse(true);
  return true;
#else
  return false;
#endif
}

CloudPrintPrivateGetHostNameFunction::CloudPrintPrivateGetHostNameFunction() {
}

CloudPrintPrivateGetHostNameFunction::~CloudPrintPrivateGetHostNameFunction() {
}

bool CloudPrintPrivateGetHostNameFunction::RunAsync() {
  SetResult(new base::StringValue(
      CloudPrintTestsDelegate::instance() ?
      CloudPrintTestsDelegate::instance()->GetHostName() :
      net::GetHostName()));
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
#if defined(ENABLE_FULL_PRINTING)
  std::vector<std::string> result;
  if (CloudPrintTestsDelegate::instance()) {
    SendResults(CloudPrintTestsDelegate::instance()->GetPrinters());
  } else {
    CloudPrintProxyService* service =
        CloudPrintProxyServiceFactory::GetForProfile(GetProfile());
    if (!service)
      return false;
    service->GetPrinters(
        base::Bind(&CloudPrintPrivateGetPrintersFunction::SendResults, this));
  }
  return true;
#else
  return false;
#endif
}


CloudPrintPrivateGetClientIdFunction::CloudPrintPrivateGetClientIdFunction() {
}

CloudPrintPrivateGetClientIdFunction::~CloudPrintPrivateGetClientIdFunction() {
}

bool CloudPrintPrivateGetClientIdFunction::RunAsync() {
  SetResult(new base::StringValue(
      CloudPrintTestsDelegate::instance() ?
      CloudPrintTestsDelegate::instance()->GetClientId() :
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_CLOUD_PRINT)));
  SendResponse(true);
  return true;
}

}  // namespace extensions
