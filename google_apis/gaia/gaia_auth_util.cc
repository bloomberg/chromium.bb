// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_util.h"

#include <vector>

#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"

namespace gaia {

namespace {
const char kGmailDomain[] = "gmail.com";
}

std::string CanonicalizeEmail(const std::string& email_address) {
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

std::string CanonicalizeDomain(const std::string& domain) {
  // Canonicalization of domain names means lower-casing them. Make sure to
  // update this function in sync with Canonicalize if this ever changes.
  return StringToLowerASCII(domain);
}

std::string SanitizeEmail(const std::string& email_address) {
  std::string sanitized(email_address);

  // Apply a default domain if necessary.
  if (sanitized.find('@') == std::string::npos) {
    sanitized += '@';
    sanitized += kGmailDomain;
  }

  return sanitized;
}

std::string ExtractDomainName(const std::string& email_address) {
  // First canonicalize which will also verify we have proper domain part.
  std::string email = CanonicalizeEmail(email_address);
  size_t separator_pos = email.find('@');
  if (separator_pos != email.npos && separator_pos < email.length() - 1)
    return email.substr(separator_pos + 1);
  else
    NOTREACHED() << "Not a proper email address: " << email;
  return std::string();
}

}  // namespace gaia
