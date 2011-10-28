// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_helpers.h"

#include "base/json/json_reader.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_token_store.h"
#include "chrome/service/service_process.h"

std::string StringFromJobStatus(cloud_print::PrintJobStatus status) {
  std::string ret;
  switch (status) {
    case cloud_print::PRINT_JOB_STATUS_IN_PROGRESS:
      ret = "in_progress";
      break;
    case cloud_print::PRINT_JOB_STATUS_ERROR:
      ret = "error";
      break;
    case cloud_print::PRINT_JOB_STATUS_COMPLETED:
      ret = "done";
      break;
    default:
      ret = "unknown";
      NOTREACHED();
      break;
  }
  return ret;
}

// Appends a relative path to the url making sure to append a '/' if the
// URL's path does not end with a slash. It is assumed that |path| does not
// begin with a '/'.
// NOTE: Since we ALWAYS want to append here, we simply append the path string
// instead of calling url_utils::ResolveRelative. The input |url| may or may not
// contain a '/' at the end.
std::string AppendPathToUrl(const GURL& url, const std::string& path) {
  DCHECK(path[0] != '/');
  std::string ret = url.path();
  if (url.has_path() && (ret[ret.length() - 1] != '/')) {
    ret += '/';
  }
  ret += path;
  return ret;
}

GURL CloudPrintHelpers::GetUrlForPrinterRegistration(
    const GURL& cloud_print_server_url) {
  std::string path(AppendPathToUrl(cloud_print_server_url, "register"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForPrinterUpdate(
    const GURL& cloud_print_server_url, const std::string& printer_id) {
  std::string path(AppendPathToUrl(cloud_print_server_url, "update"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("printerid=%s", printer_id.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForPrinterDelete(
    const GURL& cloud_print_server_url, const std::string& printer_id) {
  std::string path(AppendPathToUrl(cloud_print_server_url, "delete"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("printerid=%s", printer_id.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForPrinterList(const GURL& cloud_print_server_url,
                                             const std::string& proxy_id) {
  std::string path(AppendPathToUrl(cloud_print_server_url, "list"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("proxy=%s", proxy_id.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForJobFetch(const GURL& cloud_print_server_url,
                                          const std::string& printer_id,
                                          const std::string& reason) {
  std::string path(AppendPathToUrl(cloud_print_server_url, "fetch"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("printerid=%s&deb=%s",
                                   printer_id.c_str(),
                                   reason.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForJobStatusUpdate(
    const GURL& cloud_print_server_url, const std::string& job_id,
    cloud_print::PrintJobStatus status) {
  std::string status_string = StringFromJobStatus(status);
  std::string path(AppendPathToUrl(cloud_print_server_url, "control"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("jobid=%s&status=%s",
                                   job_id.c_str(), status_string.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL CloudPrintHelpers::GetUrlForJobStatusUpdate(
    const GURL& cloud_print_server_url, const std::string& job_id,
    const cloud_print::PrintJobDetails& details) {
  std::string status_string = StringFromJobStatus(details.status);
  std::string path(AppendPathToUrl(cloud_print_server_url, "control"));
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
  std::string path(AppendPathToUrl(cloud_print_server_url, "user/message"));
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
  std::string path(AppendPathToUrl(cloud_print_server_url, "createrobot"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("oauth_client_id=%s&proxy=%s",
                                    oauth_client_id.c_str(),
                                    proxy_id.c_str());
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}


bool CloudPrintHelpers::ParseResponseJSON(
    const std::string& response_data, bool* succeeded,
    DictionaryValue** response_dict) {
  scoped_ptr<Value> message_value(base::JSONReader::Read(response_data, false));
  if (!message_value.get())
    return false;

  if (!message_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  scoped_ptr<DictionaryValue> response_dict_local(
      static_cast<DictionaryValue*>(message_value.release()));
  if (succeeded) {
    if (!response_dict_local->GetBoolean(kSuccessValue, succeeded))
      *succeeded = false;
  }
  if (response_dict)
    *response_dict = response_dict_local.release();
  return true;
}

void CloudPrintHelpers::AddMultipartValueForUpload(
    const std::string& value_name, const std::string& value,
    const std::string& mime_boundary, const std::string& content_type,
    std::string* post_data) {
  DCHECK(post_data);
  // First line is the boundary
  post_data->append("--" + mime_boundary + "\r\n");
  // Next line is the Content-disposition
  post_data->append(StringPrintf("Content-Disposition: form-data; "
                   "name=\"%s\"\r\n", value_name.c_str()));
  if (!content_type.empty()) {
    // If Content-type is specified, the next line is that
    post_data->append(StringPrintf("Content-Type: %s\r\n",
                      content_type.c_str()));
  }
  // Leave an empty line and append the value.
  post_data->append(StringPrintf("\r\n%s\r\n", value.c_str()));
}

// Create a MIME boundary marker (27 '-' characters followed by 16 hex digits).
void CloudPrintHelpers::CreateMimeBoundaryForUpload(std::string* out) {
  int r1 = base::RandInt(0, kint32max);
  int r2 = base::RandInt(0, kint32max);
  base::SStringPrintf(out, "---------------------------%08X%08X", r1, r2);
}

std::string CloudPrintHelpers::GenerateHashOfStringMap(
    const std::map<std::string, std::string>& string_map) {
  std::string values_list;
  std::map<std::string, std::string>::const_iterator it;
  for (it = string_map.begin(); it != string_map.end(); ++it) {
    values_list.append(it->first);
    values_list.append(it->second);
  }
  return base::MD5String(values_list);
}

void CloudPrintHelpers::GenerateMultipartPostDataForPrinterTags(
    const std::map<std::string, std::string>& printer_tags,
    const std::string& mime_boundary,
    std::string* post_data) {
  // We do not use the GenerateHashOfStringMap to compute the hash because that
  // method iterates through the map again. Since we are already iterating here
  // we just compute it on the fly.
  // Also, in some cases, the code that calls this has already computed the
  // hash. We could take in the precomputed hash as an argument but that just
  // makes the code more complicated.
  std::string tags_list;
  std::map<std::string, std::string>::const_iterator it;
  for (it = printer_tags.begin(); it != printer_tags.end(); ++it) {
    // TODO(gene) Escape '=' char from name. Warning for now.
    if (it->first.find('=') != std::string::npos) {
      LOG(WARNING) <<
          "CP_PROXY: Printer option name contains '=' character";
      NOTREACHED();
    }
    tags_list.append(it->first);
    tags_list.append(it->second);

    // All our tags have a special prefix to identify them as such.
    std::string msg(kProxyTagPrefix);
    msg += it->first;
    msg += "=";
    msg += it->second;
    AddMultipartValueForUpload(kPrinterTagValue, msg, mime_boundary,
                               std::string(), post_data);
  }
  std::string tags_hash = base::MD5String(tags_list);
  std::string tags_hash_msg(kTagsHashTagName);
  tags_hash_msg += "=";
  tags_hash_msg += tags_hash;
  AddMultipartValueForUpload(kPrinterTagValue, tags_hash_msg, mime_boundary,
                             std::string(), post_data);
}

bool CloudPrintHelpers::IsDryRunJob(const std::vector<std::string>& tags) {
  std::vector<std::string>::const_iterator it;
  for (it = tags.begin(); it != tags.end(); ++it) {
    if (*it == kTagDryRunFlag)
      return true;
  }
  return false;
}

std::string CloudPrintHelpers::GetCloudPrintAuthHeader() {
  std::string header;
  CloudPrintTokenStore* token_store = CloudPrintTokenStore::current();
  if (!token_store || token_store->token().empty()) {
    // Using LOG here for critical errors. GCP connector may run in the headless
    // mode and error indication might be useful for user in that case.
    LOG(ERROR) << "CP_PROXY: Missing OAuth token for request";
  }

  if (token_store) {
    header = "Authorization: OAuth ";
    header += token_store->token();
  }
  return header;
}

