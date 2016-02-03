// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_HELPER_H_
#define CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_HELPER_H_

// A class that provides common functionality for safebrowsing protocol managers
// that communicate with Google servers.

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace safe_browsing {

struct SafeBrowsingProtocolConfig {
  SafeBrowsingProtocolConfig();
  ~SafeBrowsingProtocolConfig();
  std::string client_name;
  std::string url_prefix;
  std::string backup_connect_error_url_prefix;
  std::string backup_http_error_url_prefix;
  std::string backup_network_error_url_prefix;
  std::string version;
  bool disable_auto_update;
};

class SafeBrowsingProtocolManagerHelper {
 public:
  // returns version
  static std::string Version();

  // Composes a URL using |prefix|, |method| (e.g.: gethash, download, report).
  // |client_name| and |version|. When not empty, |additional_query| is
  // appended to the URL with an additional "&" in the front.
  static std::string ComposeUrl(const std::string& prefix,
                                const std::string& method,
                                const std::string& client_name,
                                const std::string& version,
                                const std::string& additional_query);

  // Similar to above function, and appends "&ext=1" at the end of URL if
  // |is_extended_reporting| is true, otherwise, appends "&ext=0".
  static std::string ComposeUrl(const std::string& prefix,
                                const std::string& method,
                                const std::string& client_name,
                                const std::string& version,
                                const std::string& additional_query,
                                bool is_extended_reporting);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SafeBrowsingProtocolManagerHelper);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_HELPER_H_
