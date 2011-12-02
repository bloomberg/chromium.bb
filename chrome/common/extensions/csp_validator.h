// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CSP_VALIDATOR_H_
#define CHROME_COMMON_EXTENSIONS_CSP_VALIDATOR_H_
#pragma once

#include <string>

namespace extensions {

namespace csp_validator {

// Checks whether the given |policy| is legal for use in the extension system.
// This check just ensures that the policy doesn't contain any characters that
// will cause problems when we transmit the policy in an HTTP header.
bool ContentSecurityPolicyIsLegal(const std::string& policy);

// Checks whether the given |policy| meets the minimum security requirements
// for use in the extension system. The philosophy behind our minimum
// requirements is that an XSS vulnerability in the extension should not be
// able to execute script, even in the precense of an active network attacker.
// Specifically, 'unsafe-inline' and 'unsafe-eval' are forbidden, as is
// script or object inclusion from insecure schemes. Also, the use of * is
// forbidden for scripts and objects.
bool ContentSecurityPolicyIsSecure(const std::string& policy);

}  // namespace csp_validator

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_CSP_VALIDATOR_H_
