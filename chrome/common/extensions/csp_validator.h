// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CSP_VALIDATOR_H_
#define CHROME_COMMON_EXTENSIONS_CSP_VALIDATOR_H_

#include <string>

#include "chrome/common/extensions/extension.h"

namespace extensions {

namespace csp_validator {

// Checks whether the given |policy| is legal for use in the extension system.
// This check just ensures that the policy doesn't contain any characters that
// will cause problems when we transmit the policy in an HTTP header.
bool ContentSecurityPolicyIsLegal(const std::string& policy);

// Checks whether the given |policy| meets the minimum security requirements
// for use in the extension system.
//
// Ideally, we would like to say that an XSS vulnerability in the extension
// should not be able to execute script, even in the precense of an active
// network attacker.
//
// However, we found that it broke too many deployed extensions to limit
// 'unsafe-eval' in the script-src directive, so that is allowed as a special
// case for extensions. Platform apps disallow it.
bool ContentSecurityPolicyIsSecure(
    const std::string& policy, Extension::Type type);

// Checks whether the given |policy| enforces a unique origin sandbox as
// defined by http://www.whatwg.org/specs/web-apps/current-work/multipage/
// the-iframe-element.html#attr-iframe-sandbox. The policy must have the
// "sandbox" directive, and the sandbox tokens must not include
// "allow-same-origin". Additional restrictions may be imposed depending on
// |type|.
bool ContentSecurityPolicyIsSandboxed(
    const std::string& policy, Extension::Type type);

}  // namespace csp_validator

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_CSP_VALIDATOR_H_
