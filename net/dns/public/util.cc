// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/util.h"

#include <set>
#include <unordered_map>

#include "net/third_party/uri_template/uri_template.h"
#include "url/gurl.h"

namespace net {
namespace dns_util {

bool IsValidDoHTemplate(const string& server_template,
                        const string& server_method) {
  std::string url_string;
  std::string test_query = "this_is_a_test_query";
  std::unordered_map<std::string, std::string> template_params(
      {{"dns", test_query}});
  std::set<std::string> vars_found;
  bool valid_template = uri_template::Expand(server_template, template_params,
                                             &url_string, &vars_found);
  if (!valid_template) {
    // The URI template is malformed.
    return false;
  }
  if (server_method != "POST" && vars_found.find("dns") == vars_found.end()) {
    // GET requests require the template to have a dns variable.
    return false;
  }
  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIs("https")) {
    // The expanded template must be a valid HTTPS URL.
    return false;
  }
  if (url.host().find(test_query) != std::string::npos) {
    // The dns variable may not be part of the hostname.
    return false;
  }
  return true;
}

}  // namespace dns_util
}  // namespace net
