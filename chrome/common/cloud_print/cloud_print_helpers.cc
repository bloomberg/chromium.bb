// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/cloud_print/cloud_print_helpers.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"

namespace cloud_print {

const char kPrinterListValue[] = "printers";
const char kSuccessValue[] = "success";

// Certain cloud print requests require Chrome's X-CloudPrint-Proxy header.
const char kChromeCloudPrintProxyHeader[] = "X-CloudPrint-Proxy: Chrome";

std::string AppendPathToUrl(const GURL& url, const std::string& path) {
  DCHECK_NE(path[0], '/');
  std::string ret = url.path();
  if (url.has_path() && (ret[ret.length() - 1] != '/'))
    ret += '/';
  ret += path;
  return ret;
}

GURL GetUrlForSearch(const GURL& cloud_print_server_url) {
  std::string path(AppendPathToUrl(cloud_print_server_url, "search"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

GURL GetUrlForSubmit(const GURL& cloud_print_server_url) {
  std::string path(AppendPathToUrl(cloud_print_server_url, "submit"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

bool ParseResponseJSON(const std::string& response_data,
                       bool* succeeded,
                       DictionaryValue** response_dict) {
  scoped_ptr<Value> message_value(base::JSONReader::Read(response_data));
  if (!message_value.get())
    return false;

  if (!message_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  scoped_ptr<DictionaryValue> response_dict_local(
      static_cast<DictionaryValue*>(message_value.release()));
  if (succeeded &&
      !response_dict_local->GetBoolean(cloud_print::kSuccessValue, succeeded))
    *succeeded = false;
  if (response_dict)
    *response_dict = response_dict_local.release();
  return true;
}

void AddMultipartValueForUpload(const std::string& value_name,
                                const std::string& value,
                                const std::string& mime_boundary,
                                const std::string& content_type,
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
void CreateMimeBoundaryForUpload(std::string* out) {
  int r1 = base::RandInt(0, kint32max);
  int r2 = base::RandInt(0, kint32max);
  base::SStringPrintf(out, "---------------------------%08X%08X", r1, r2);
}

}  // namespace cloud_print
