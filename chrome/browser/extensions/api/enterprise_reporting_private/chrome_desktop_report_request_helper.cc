// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"

#include <string>

#include "base/base_paths.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/policy_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"

namespace em = enterprise_management;

namespace extensions {
namespace {

// JSON key in extension arguments.
const char kMachineName[] = "machineName";
const char kOSInfo[] = "osInfo";
const char kOSUser[] = "osUser";
const char kBrowserReport[] = "browserReport";
const char kChromeUserProfileReport[] = "chromeUserProfileReport";
const char kChromeSignInUser[] = "chromeSignInUser";
const char kExtensionData[] = "extensionData";
const char kPlugins[] = "plugins";

// JSON key in the os_info field.
const char kOS[] = "os";
const char kOSVersion[] = "os_version";

const char kDefaultDictionary[] = "{}";
const char kDefaultList[] = "[]";

enum Type {
  LIST,
  DICTIONARY,
};

std::string GetChromePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AsUTF8Unsafe();
}

std::string GetProfileId(const Profile* profile) {
  return profile->GetOriginalProfile()->GetPath().AsUTF8Unsafe();
}

// Returns last policy fetch timestamp of machine level user cloud policy if
// it exists. Otherwise, returns zero.
int64_t GetMachineLevelUserCloudPolicyFetchTimestamp() {
  policy::MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->GetMachineLevelUserCloudPolicyManager();
  if (!manager || !manager->IsClientRegistered())
    return 0;
  return manager->core()->client()->last_policy_timestamp().ToJavaTime();
}

void AppendAdditionalBrowserInformation(em::ChromeDesktopReportRequest* request,
                                        Profile* profile) {
  // Set Chrome version number
  request->mutable_browser_report()->set_browser_version(
      version_info::GetVersionNumber());
  // Set Chrome channel
  request->mutable_browser_report()->set_channel(
      static_cast<em::BrowserReport_Channel>(chrome::GetChannel()));
  // Set Chrome executable path
  request->mutable_browser_report()->set_executable_path(GetChromePath());

  // Add a new profile report if extension doesn't report any profile.
  if (request->browser_report().chrome_user_profile_reports_size() == 0)
    request->mutable_browser_report()->add_chrome_user_profile_reports();

  DCHECK_EQ(1, request->browser_report().chrome_user_profile_reports_size());
  // Set profile ID for the first profile.
  request->mutable_browser_report()
      ->mutable_chrome_user_profile_reports(0)
      ->set_id(GetProfileId(profile));

  // Set policy data of the first profile. Extension will report this data in
  // the future.
  request->mutable_browser_report()
      ->mutable_chrome_user_profile_reports(0)
      ->set_policy_data(policy::GetAllPolicyValuesAsJSON(profile, true));

  int64_t timestamp = GetMachineLevelUserCloudPolicyFetchTimestamp();
  if (timestamp > 0) {
    request->mutable_browser_report()
        ->mutable_chrome_user_profile_reports(0)
        ->set_policy_fetched_timestamp(timestamp);
  }

  // Set the profile name
  request->mutable_browser_report()
      ->mutable_chrome_user_profile_reports(0)
      ->set_name(profile->GetProfileUserName());
}

bool UpdateJSONEncodedStringEntry(const base::Value& dict_value,
                                  const char key[],
                                  std::string* entry,
                                  const Type type) {
  if (const base::Value* value = dict_value.FindKey(key)) {
    if ((type == DICTIONARY && !value->is_dict()) ||
        (type == LIST && !value->is_list())) {
      return false;
    }
    base::JSONWriter::Write(*value, entry);
  } else {
    if (type == DICTIONARY)
      *entry = kDefaultDictionary;
    else if (type == LIST)
      *entry = kDefaultList;
  }

  return true;
}

bool UpdateOSInfoEntry(const base::Value& report, std::string* os_info_string) {
  base::Value writable_os_info(base::Value::Type::DICTIONARY);
  if (const base::Value* os_info = report.FindKey(kOSInfo)) {
    if (!os_info->is_dict())
      return false;
    writable_os_info = os_info->Clone();
  }
  writable_os_info.SetKey(kOS, base::Value(policy::GetOSPlatform()));
  writable_os_info.SetKey(kOSVersion, base::Value(policy::GetOSVersion()));
  base::JSONWriter::Write(writable_os_info, os_info_string);
  return true;
}

std::unique_ptr<em::ChromeUserProfileReport>
GenerateChromeUserProfileReportRequest(const base::Value& profile_report) {
  if (!profile_report.is_dict())
    return nullptr;

  std::unique_ptr<em::ChromeUserProfileReport> request =
      std::make_unique<em::ChromeUserProfileReport>();

  if (!UpdateJSONEncodedStringEntry(profile_report, kChromeSignInUser,
                                    request->mutable_chrome_signed_in_user(),
                                    DICTIONARY) ||
      !UpdateJSONEncodedStringEntry(profile_report, kExtensionData,
                                    request->mutable_extension_data(), LIST) ||
      !UpdateJSONEncodedStringEntry(profile_report, kPlugins,
                                    request->mutable_plugins(), LIST)) {
    return nullptr;
  }

  return request;
}

}  // namespace

std::unique_ptr<em::ChromeDesktopReportRequest>
GenerateChromeDesktopReportRequest(const base::DictionaryValue& report,
                                   Profile* profile) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      std::make_unique<em::ChromeDesktopReportRequest>();

  if (!UpdateJSONEncodedStringEntry(
          report, kMachineName, request->mutable_machine_name(), DICTIONARY) ||
      !UpdateJSONEncodedStringEntry(report, kOSUser, request->mutable_os_user(),
                                    DICTIONARY) ||
      !UpdateOSInfoEntry(report, request->mutable_os_info())) {
    return nullptr;
  }

  if (const base::Value* browser_report =
          report.FindKeyOfType(kBrowserReport, base::Value::Type::DICTIONARY)) {
    if (const base::Value* profile_reports = browser_report->FindKeyOfType(
            kChromeUserProfileReport, base::Value::Type::LIST)) {
      if (profile_reports->GetList().size() > 0) {
        DCHECK_EQ(1u, profile_reports->GetList().size());
        // Currently, profile send their browser reports individually.
        std::unique_ptr<em::ChromeUserProfileReport> profile_report_request =
            GenerateChromeUserProfileReportRequest(
                profile_reports->GetList()[0]);
        if (!profile_report_request)
          return nullptr;
        request->mutable_browser_report()
            ->mutable_chrome_user_profile_reports()
            ->AddAllocated(profile_report_request.release());
      }
    }
  }

  AppendAdditionalBrowserInformation(request.get(), profile);

  return request;
}

}  // namespace extensions
