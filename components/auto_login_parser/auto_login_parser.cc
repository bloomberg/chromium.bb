// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/auto_login_parser/auto_login_parser.h"

#include <utility>
#include <vector>

#include "base/string_split.h"
#include "net/base/escape.h"

namespace components {
namespace auto_login {

HeaderData::HeaderData() {}
HeaderData::~HeaderData() {}

// static
bool ParseHeader(const std::string& header, HeaderData* header_data) {
  // TODO(pliard): Investigate/fix potential internationalization issue. It
  // seems that "account" from the x-auto-login header might contain non-ASCII
  // characters.
  if (header.empty())
    return false;

  std::vector<std::pair<std::string, std::string> > pairs;
  if (!base::SplitStringIntoKeyValuePairs(header, '=', '&', &pairs))
    return false;

  // Parse the information from the |header| string.
  HeaderData local_params;
  for (size_t i = 0; i < pairs.size(); ++i) {
    const std::pair<std::string, std::string>& pair = pairs[i];
    std::string unescaped_value(net::UnescapeURLComponent(
          pair.second, net::UnescapeRule::URL_SPECIAL_CHARS));
    if (pair.first == "realm") {
      // Currently we only accept GAIA credentials.
      if (unescaped_value != "com.google")
        return false;
      local_params.realm = unescaped_value;
    } else if (pair.first == "account") {
      local_params.account = unescaped_value;
    } else if (pair.first == "args") {
      local_params.args = unescaped_value;
    }
  }
  if (local_params.realm.empty() || local_params.args.empty())
    return false;

  *header_data = local_params;
  return true;
}

}  // namespace auto_login
}  // namespace components
