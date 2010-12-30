// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

// static
const char Authenticator::kSpecialCaseDomain[] = "gmail.com";

Authenticator::Authenticator(LoginStatusConsumer* consumer)
    : consumer_(consumer) {
}

Authenticator::~Authenticator() {}

// static
std::string Authenticator::Canonicalize(const std::string& email_address) {
  std::vector<std::string> parts;
  char at = '@';
  base::SplitString(email_address, at, &parts);
  DCHECK_EQ(parts.size(), 2U) << "email_address should have only one @";
  if (parts[1] == kSpecialCaseDomain)  // only strip '.' for gmail accounts.
    RemoveChars(parts[0], ".", &parts[0]);
  // Technically the '+' handling here could be removed, as the google
  // account servers do not tolerate them, so we don't need to either.
  // TODO(cmasone): remove this, unless this code becomes obsolete altogether.
  if (parts[0].find('+') != std::string::npos)
    parts[0].erase(parts[0].find('+'));
  std::string new_email = StringToLowerASCII(JoinString(parts, at));
  VLOG(1) << "Canonicalized " << email_address << " to " << new_email;
  return new_email;
}

}  // namespace chromeos
