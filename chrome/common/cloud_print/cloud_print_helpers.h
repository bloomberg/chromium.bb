// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_
#define CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_

#include <string>

class GURL;

namespace base {
class DictionaryValue;
}

// Helper consts and methods for both cloud print and chrome browser.
namespace cloud_print {

// Values in the respone JSON from the cloud print server
extern const char kPrinterListValue[];
extern const char kSuccessValue[];

extern const char kChromeCloudPrintProxyHeader[];

// Appends a relative path to the url making sure to append a '/' if the
// URL's path does not end with a slash. It is assumed that |path| does not
// begin with a '/'.
// NOTE: Since we ALWAYS want to append here, we simply append the path string
// instead of calling url_utils::ResolveRelative. The input |url| may or may not
// contain a '/' at the end.
std::string AppendPathToUrl(const GURL& url, const std::string& path);

GURL GetUrlForSearch(const GURL& cloud_print_server_url);
GURL GetUrlForSubmit(const GURL& cloud_print_server_url);

// Parses the response data for any cloud print server request. The method
// returns false if there was an error in parsing the JSON. The succeeded
// value returns the value of the "success" value in the response JSON.
// Returns the response as a dictionary value.
bool ParseResponseJSON(const std::string& response_data,
                       bool* succeeded,
                       base::DictionaryValue** response_dict);

// Prepares one value as part of a multi-part upload request.
void AddMultipartValueForUpload(const std::string& value_name,
                                const std::string& value,
                                const std::string& mime_boundary,
                                const std::string& content_type,
                                std::string* post_data);

// Create a MIME boundary marker (27 '-' characters followed by 16 hex digits).
void CreateMimeBoundaryForUpload(std::string *out);

}  // namespace cloud_print

#endif  // CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_
