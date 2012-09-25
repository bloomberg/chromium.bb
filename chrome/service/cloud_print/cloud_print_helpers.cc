// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_helpers.h"

#include "base/json/json_reader.h"
#include "base/md5.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_token_store.h"
#include "chrome/service/service_process.h"

namespace {

typedef std::map<std::string, std::string> PrinterTags;

void GetPrinterTags(const printing::PrinterBasicInfo& printer,
                    PrinterTags* printer_tags) {
  *printer_tags = printer.options;
  chrome::VersionInfo version_info;
  DCHECK(version_info.is_valid());
  (*printer_tags)[kChromeVersionTagName] = version_info.CreateVersionString();
  using base::SysInfo;
  (*printer_tags)[kSystemNameTagName] = SysInfo::OperatingSystemName();
  (*printer_tags)[kSystemVersionTagName] = SysInfo::OperatingSystemVersion();
}

std::string HashPrinterTags(const PrinterTags& strings) {
  std::string values_list;
  PrinterTags::const_iterator it;
  for (it = strings.begin(); it != strings.end(); ++it) {
    values_list.append(it->first);
    values_list.append(it->second);
  }
  return base::MD5String(values_list);
}

}  // namespace

std::string StringFromJobStatus(cloud_print::PrintJobStatus status) {
  std::string ret;
  switch (status) {
    case cloud_print::PRINT_JOB_STATUS_IN_PROGRESS:
      ret = "IN_PROGRESS";
      break;
    case cloud_print::PRINT_JOB_STATUS_ERROR:
      ret = "ERROR";
      break;
    case cloud_print::PRINT_JOB_STATUS_COMPLETED:
      ret = "DONE";
      break;
    default:
      ret = "UNKNOWN";
      NOTREACHED();
      break;
  }
  return ret;
}

GURL CloudPrintHelpers::GetUrlForPrinterRegistration(
    const GURL& cloud_print_server_url) {
  std::string path(
      cloud_print::AppendPathToUrl(cloud_print_server_url, "register"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForPrinterUpdate(
    const GURL& cloud_print_server_url,
    const std::string& printer_id) {
  std::string path(
      cloud_print::AppendPathToUrl(cloud_print_server_url, "update"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("printerid=%s", printer_id.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForPrinterDelete(
    const GURL& cloud_print_server_url,
    const std::string& printer_id,
    const std::string& reason) {
  std::string path(
      cloud_print::AppendPathToUrl(cloud_print_server_url, "delete"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("printerid=%s&reason=%s",
      printer_id.c_str(), reason.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForPrinterList(const GURL& cloud_print_server_url,
                                             const std::string& proxy_id) {
  std::string path(
      cloud_print::AppendPathToUrl(cloud_print_server_url, "list"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("proxy=%s", proxy_id.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForJobFetch(const GURL& cloud_print_server_url,
                                          const std::string& printer_id,
                                          const std::string& reason) {
  std::string path(
      cloud_print::AppendPathToUrl(cloud_print_server_url, "fetch"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("printerid=%s&deb=%s",
                                   printer_id.c_str(),
                                   reason.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForJobStatusUpdate(
    const GURL& cloud_print_server_url,
    const std::string& job_id,
    cloud_print::PrintJobStatus status) {
  std::string status_string = StringFromJobStatus(status);
  std::string path(
      cloud_print::AppendPathToUrl(cloud_print_server_url, "control"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("jobid=%s&status=%s",
                                   job_id.c_str(), status_string.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForJobStatusUpdate(
    const GURL& cloud_print_server_url,
    const std::string& job_id,
    const cloud_print::PrintJobDetails& details) {
  std::string status_string = StringFromJobStatus(details.status);
  std::string path(
      cloud_print::AppendPathToUrl(cloud_print_server_url, "control"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query =
      StringPrintf("jobid=%s&status=%s&code=%d&message=%s"
                   "&numpages=%d&pagesprinted=%d",
                   job_id.c_str(),
                   status_string.c_str(),
                   details.platform_status_flags,
                   details.status_message.c_str(),
                   details.total_pages,
                   details.pages_printed);
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForUserMessage(const GURL& cloud_print_server_url,
                                             const std::string& message_id) {
  std::string path(
      cloud_print::AppendPathToUrl(cloud_print_server_url, "message"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("code=%s", message_id.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForGetAuthCode(const GURL& cloud_print_server_url,
                                             const std::string& oauth_client_id,
                                             const std::string& proxy_id) {
  // We use the internal API "createrobot" instead of "getauthcode". This API
  // will add the robot as owner to all the existing printers for this user.
  std::string path(
    cloud_print::AppendPathToUrl(cloud_print_server_url, "createrobot"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("oauth_client_id=%s&proxy=%s",
                                    oauth_client_id.c_str(),
                                    proxy_id.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

std::string CloudPrintHelpers::GetHashOfPrinterTags(
    const printing::PrinterBasicInfo& printer) {
  PrinterTags printer_tags;
  GetPrinterTags(printer, &printer_tags);
  std::string values_list;
  for (PrinterTags::const_iterator it = printer_tags.begin();
      it != printer_tags.end(); ++it) {
    values_list.append(it->first);
    values_list.append(it->second);
  }
  return base::MD5String(values_list);
}

std::string CloudPrintHelpers::GetPostDataForPrinterTags(
    const printing::PrinterBasicInfo& printer,
    const std::string& mime_boundary) {
  PrinterTags printer_tags;
  GetPrinterTags(printer, &printer_tags);
  std::string post_data;
  for (PrinterTags::const_iterator it = printer_tags.begin();
      it != printer_tags.end(); ++it) {
    // TODO(gene) Escape '=' char from name. Warning for now.
    if (it->first.find('=') != std::string::npos) {
      LOG(WARNING) <<
          "CP_PROXY: Printer option name contains '=' character";
      NOTREACHED();
    }
    // All our tags have a special prefix to identify them as such.
    std::string msg(kProxyTagPrefix);
    msg += it->first;
    msg += "=";
    msg += it->second;
    cloud_print::AddMultipartValueForUpload(kPrinterTagValue, msg,
        mime_boundary, std::string(), &post_data);
  }
  std::string tags_hash_msg(kTagsHashTagName);
  tags_hash_msg += "=";
  tags_hash_msg += HashPrinterTags(printer_tags);
  cloud_print::AddMultipartValueForUpload(kPrinterTagValue, tags_hash_msg,
                                          mime_boundary, std::string(),
                                          &post_data);
  return post_data;
}

bool CloudPrintHelpers::IsDryRunJob(const std::vector<std::string>& tags) {
  std::vector<std::string>::const_iterator it;
  for (it = tags.begin(); it != tags.end(); ++it) {
    if (*it == kTagDryRunFlag)
      return true;
  }
  return false;
}

std::string CloudPrintHelpers::GetCloudPrintAuthHeaderFromStore() {
  CloudPrintTokenStore* token_store = CloudPrintTokenStore::current();
  if (!token_store || token_store->token().empty()) {
    // Using LOG here for critical errors. GCP connector may run in the headless
    // mode and error indication might be useful for user in that case.
    LOG(ERROR) << "CP_PROXY: Missing OAuth token for request";
    return std::string();
  }
  return GetCloudPrintAuthHeader(token_store->token());
}

std::string CloudPrintHelpers::GetCloudPrintAuthHeader(
    const std::string& auth_token) {
  std::string header;
  header = "Authorization: OAuth ";
  header += auth_token;
  return header;
}
