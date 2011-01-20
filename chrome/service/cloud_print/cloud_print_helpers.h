// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_
#pragma once

#include <map>
#include <string>

#include "chrome/service/cloud_print/print_system.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class Task;
class URLFetcher;

// Helper methods for the cloud print proxy code.
class CloudPrintHelpers {
 public:
  static GURL GetUrlForPrinterRegistration(const GURL& cloud_print_server_url);
  static GURL GetUrlForPrinterUpdate(const GURL& cloud_print_server_url,
                                     const std::string& printer_id);
  static GURL GetUrlForPrinterDelete(const GURL& cloud_print_server_url,
                                     const std::string& printer_id);
  static GURL GetUrlForPrinterList(const GURL& cloud_print_server_url,
                                   const std::string& proxy_id);
  static GURL GetUrlForJobFetch(const GURL& cloud_print_server_url,
                                const std::string& printer_id,
                                const std::string& reason);
  static GURL GetUrlForJobStatusUpdate(const GURL& cloud_print_server_url,
                                       const std::string& job_id,
                                       cloud_print::PrintJobStatus status);
  static GURL GetUrlForJobStatusUpdate(
      const GURL& cloud_print_server_url, const std::string& job_id,
      const cloud_print::PrintJobDetails& details);
  static GURL GetUrlForUserMessage(const GURL& cloud_print_server_url,
                                   const std::string& message_id);

  // Parses the response data for any cloud print server request. The method
  // returns false if there was an error in parsing the JSON. The succeeded
  // value returns the value of the "success" value in the response JSON.
  // Returns the response as a dictionary value.
  static bool ParseResponseJSON(const std::string& response_data,
      bool* succeeded, DictionaryValue** response_dict);

  // Prepares one value as part of a multi-part upload request.
  static void AddMultipartValueForUpload(
      const std::string& value_name, const std::string& value,
      const std::string& mime_boundary, const std::string& content_type,
      std::string* post_data);
// Create a MIME boundary marker (27 '-' characters followed by 16 hex digits).
  static void CreateMimeBoundaryForUpload(std::string *out);
  // Generates an MD5 hash of the contents of a string map.
  static std::string GenerateHashOfStringMap(
      const std::map<std::string, std::string>& string_map);
  static void GenerateMultipartPostDataForPrinterTags(
      const std::map<std::string, std::string>& printer_tags,
      const std::string& mime_boundary,
      std::string* post_data);

  // Returns true is tags indicate a dry run (test) job.
  static bool IsDryRunJob(const std::vector<std::string>& tags);
 private:
  CloudPrintHelpers() {
  }
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_

