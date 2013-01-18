// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTO_LOGIN_PARSER_AUTO_LOGIN_PARSER_H_
#define COMPONENTS_AUTO_LOGIN_PARSER_AUTO_LOGIN_PARSER_H_

#include <string>

namespace components {
namespace auto_login {

struct HeaderData {
  HeaderData();
  ~HeaderData();

  // "realm" string from x-auto-login (e.g. "com.google").
  std::string realm;

  // "account" string from x-auto-login.
  std::string account;

  // "args" string from x-auto-login to be passed to MergeSession. This string
  // should be considered opaque and not be cracked open to look inside.
  std::string args;
};

// Returns whether parsing succeeded. Parameter |header_data| will not be
// modified if parsing fails.
bool ParseHeader(const std::string& header, HeaderData* header_data);

}  // namespace auto_login
}  // namespace components

#endif  // COMPONENTS_AUTO_LOGIN_PARSER_AUTO_LOGIN_PARSER_H_
