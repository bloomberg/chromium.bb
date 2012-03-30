// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/authenticator.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"

namespace chromeos {

class LoginStatusConsumer;

namespace {
const char kGmailDomain[] = "gmail.com";
}

Authenticator::Authenticator(LoginStatusConsumer* consumer)
    : consumer_(consumer), authentication_profile_(NULL) {
}

Authenticator::~Authenticator() {}

// static
std::string Authenticator::Canonicalize(const std::string& email_address) {
  std::vector<std::string> parts;
  char at = '@';
  base::SplitString(email_address, at, &parts);
  if (parts.size() != 2U)
    NOTREACHED() << "expecting exactly one @, but got " << parts.size();
  else if (parts[1] == kGmailDomain)  // only strip '.' for gmail accounts.
    RemoveChars(parts[0], ".", &parts[0]);
  std::string new_email = StringToLowerASCII(JoinString(parts, at));
  VLOG(1) << "Canonicalized " << email_address << " to " << new_email;
  return new_email;
}

// static
std::string Authenticator::Sanitize(const std::string& email_address) {
  std::string sanitized(email_address);

  // Apply a default domain if necessary.
  if (sanitized.find('@') == std::string::npos) {
    sanitized += '@';
    sanitized += kGmailDomain;
  }

  return sanitized;
}

}  // namespace chromeos
