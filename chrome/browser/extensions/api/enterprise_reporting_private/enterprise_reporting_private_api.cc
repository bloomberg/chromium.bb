// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace extensions {
namespace {

const char kMachineName[] = "machineName";
const char kOSInfo[] = "osInfo";
const char kOSUser[] = "osUser";

bool UpdateJSONEncodedStringEntry(const base::Value& dict_value,
                                  const char key[],
                                  std::string* entry) {
  if (const base::Value* value = dict_value.FindKey(key)) {
    if (!value->is_dict() && !value->is_list())
      return false;
    base::JSONWriter::Write(*value, entry);
  }
  return true;
}

// Transfer the input from Json file to protobuf. Return nullptr if the input
// is not valid.
std::unique_ptr<enterprise_management::ChromeDesktopReportRequest>
GenerateChromeDesktopReportRequest(const base::DictionaryValue& report) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      std::make_unique<em::ChromeDesktopReportRequest>();

  if (!UpdateJSONEncodedStringEntry(report, kMachineName,
                                    request->mutable_machine_name()) ||
      !UpdateJSONEncodedStringEntry(report, kOSInfo,
                                    request->mutable_os_info()) ||
      !UpdateJSONEncodedStringEntry(report, kOSUser,
                                    request->mutable_os_user())) {
    return nullptr;
  }
  return request;
}

}  // namespace

namespace enterprise_reporting {

const char kInvalidInputErrorMessage[] = "The report is not valid.";
const char kUploadFailed[] = "Failed to upload the report.";
const char kDeviceNotEnrolled[] = "This device has not been enrolled yet.";

}  // namespace enterprise_reporting

EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    EnterpriseReportingPrivateUploadChromeDesktopReportFunction() {
  policy::DeviceManagementService* device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();
  // Initial the DeviceManagementService if it exist and hasn't been initialized
  if (device_management_service)
    device_management_service->ScheduleInitialization(0);
  cloud_policy_client_ = std::make_unique<policy::CloudPolicyClient>(
      std::string(), std::string(), device_management_service,
      g_browser_process->system_request_context(), nullptr,
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  dm_token_ = policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  client_id_ = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
}

EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    ~EnterpriseReportingPrivateUploadChromeDesktopReportFunction() {}

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateUploadChromeDesktopReportFunction::Run() {
  if (dm_token_.empty() || client_id_.empty())
    return RespondNow(Error(enterprise_reporting::kDeviceNotEnrolled));
  std::unique_ptr<
      api::enterprise_reporting_private::UploadChromeDesktopReport::Params>
      params(api::enterprise_reporting_private::UploadChromeDesktopReport::
                 Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      GenerateChromeDesktopReportRequest(params->report.additional_properties);
  if (!request) {
    return RespondNow(Error(enterprise_reporting::kInvalidInputErrorMessage));
  }

  if (!cloud_policy_client_->is_registered())
    cloud_policy_client_->SetupRegistration(dm_token_, client_id_,
                                            std::vector<std::string>());

  cloud_policy_client_->UploadChromeDesktopReport(
      std::move(request),
      base::BindRepeating(
          &EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
              OnReportUploaded,
          this));
  return RespondLater();
}

void EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    SetCloudPolicyClientForTesting(
        std::unique_ptr<policy::CloudPolicyClient> client) {
  cloud_policy_client_ = std::move(client);
}

void EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    SetRegistrationInfoForTesting(const std::string& dm_token,
                                  const std::string& client_id) {
  dm_token_ = dm_token;
  client_id_ = client_id;
}

void EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    OnResponded() {
  cloud_policy_client_.reset();
}

void EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
    OnReportUploaded(bool status) {
  if (status)
    Respond(NoArguments());
  else
    Respond(Error(enterprise_reporting::kUploadFailed));
}

}  // namespace extensions
