// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "chrome/service/cloud_print/print_system.h"
#include "googleurl/src/gurl.h"

namespace base {
class DictionaryValue;
}

// Helper methods for the cloud print proxy code.
class CloudPrintHelpers {
 public:
  static GURL GetUrlForPrinterRegistration(const GURL& cloud_print_server_url);
  static GURL GetUrlForPrinterUpdate(const GURL& cloud_print_server_url,
                                     const std::string& printer_id);
  static GURL GetUrlForPrinterDelete(const GURL& cloud_print_server_url,
                                     const std::string& printer_id,
                                     const std::string& reason);
  static GURL GetUrlForPrinterList(const GURL& cloud_print_server_url,
                                   const std::string& proxy_id);
  static GURL GetUrlForJobFetch(const GURL& cloud_print_server_url,
                                const std::string& printer_id,
                                const std::string& reason);
  static GURL GetUrlForJobStatusUpdate(const GURL& cloud_print_server_url,
                                       const std::string& job_id,
                                       cloud_print::PrintJobStatus status);
  static GURL GetUrlForJobStatusUpdate(
      const GURL& cloud_print_server_url,
      const std::string& job_id,
      const cloud_print::PrintJobDetails& details);
  static GURL GetUrlForUserMessage(const GURL& cloud_print_server_url,
                                   const std::string& message_id);
  static GURL GetUrlForGetAuthCode(const GURL& cloud_print_server_url,
                                   const std::string& oauth_client_id,
                                   const std::string& proxy_id);

  // Generates an MD5 hash of the contents of a string map.
  static std::string GenerateHashOfStringMap(
      const std::map<std::string, std::string>& string_map);
  static void GenerateMultipartPostDataForPrinterTags(
      const std::map<std::string, std::string>& printer_tags,
      const std::string& mime_boundary,
      std::string* post_data);

  // Returns true is tags indicate a dry run (test) job.
  static bool IsDryRunJob(const std::vector<std::string>& tags);

  // Created CloudPrint auth header from the auth token stored in the store.
  static std::string GetCloudPrintAuthHeaderFromStore();
  // Created CloudPrint auth header from the auth token.
  static std::string GetCloudPrintAuthHeader(const std::string& auth_token);

 private:
  CloudPrintHelpers() {}
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_
