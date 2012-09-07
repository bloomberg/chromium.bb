// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_

#include <string>

namespace gaia {

// Perform basic canonicalization of |email_address|, taking into account that
// gmail does not consider '.' or caps inside a username to matter. It also
// ignores everything after a '+'. For example, c.masone+abc@gmail.com ==
// cMaSone@gmail.com, per
// http://mail.google.com/support/bin/answer.py?hl=en&ctx=mail&answer=10313#
std::string CanonicalizeEmail(const std::string& email_address);

// Returns the canonical form of the given domain.
std::string CanonicalizeDomain(const std::string& domain);

// Sanitize emails. Currently, it only ensures all emails have a domain by
// adding gmail.com if no domain is present.
std::string SanitizeEmail(const std::string& email_address);

// Extract the domain part from the canonical form of the given email.
std::string ExtractDomainName(const std::string& email);

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_
