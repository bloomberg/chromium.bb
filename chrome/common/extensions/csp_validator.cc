// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/csp_validator.h"

#include "base/string_split.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"

namespace extensions {

namespace csp_validator {

namespace {

const char kDefaultSrc[] = "default-src";
const char kScriptSrc[] = "script-src";
const char kObjectSrc[] = "object-src";

struct DirectiveStatus {
  explicit DirectiveStatus(const char* name)
    : directive_name(name)
    , seen_in_policy(false)
    , is_secure(false) {
  }

  const char* directive_name;
  bool seen_in_policy;
  bool is_secure;
};

bool HasOnlySecureTokens(StringTokenizer& tokenizer) {
  while (tokenizer.GetNext()) {
    std::string source = tokenizer.token();
    StringToLowerASCII(&source);

    if (EndsWith(source, "*", true))
      return false;

    // We might need to relax this whitelist over time.
    if (source == "'self'" ||
        source == "'none'" ||
        StartsWithASCII(source, "https://", true) ||
        StartsWithASCII(source, "chrome://", true) ||
        StartsWithASCII(source, "chrome-extension://", true) ||
        StartsWithASCII(source, "chrome-extension-resource:", true)) {
      continue;
    }

    return false;
  }

  return true;  // Empty values default to 'none', which is secure.
}

// Returns true if |directive_name| matches |status.directive_name|.
bool UpdateStatus(const std::string& directive_name,
                  StringTokenizer& tokenizer,
                  DirectiveStatus* status) {
  if (status->seen_in_policy)
    return false;
  if (directive_name != status->directive_name)
    return false;
  status->seen_in_policy = true;
  status->is_secure = HasOnlySecureTokens(tokenizer);
  return true;
}

}  //  namespace

bool ContentSecurityPolicyIsLegal(const std::string& policy) {
  // We block these characters to prevent HTTP header injection when
  // representing the content security policy as an HTTP header.
  const char kBadChars[] = {'\r', '\n', '\0'};

  return policy.find_first_of(kBadChars, 0, arraysize(kBadChars)) ==
      std::string::npos;
}

bool ContentSecurityPolicyIsSecure(const std::string& policy) {
  // See http://www.w3.org/TR/CSP/#parse-a-csp-policy for parsing algorithm.
  std::vector<std::string> directives;
  base::SplitString(policy, ';', &directives);

  DirectiveStatus default_src_status(kDefaultSrc);
  DirectiveStatus script_src_status(kScriptSrc);
  DirectiveStatus object_src_status(kObjectSrc);

  for (size_t i = 0; i < directives.size(); ++i) {
    std::string& input = directives[i];
    StringTokenizer tokenizer(input, " \t\r\n");
    if (!tokenizer.GetNext())
      continue;

    std::string directive_name = tokenizer.token();
    StringToLowerASCII(&directive_name);

    if (UpdateStatus(directive_name, tokenizer, &default_src_status))
      continue;
    if (UpdateStatus(directive_name, tokenizer, &script_src_status))
      continue;
    if (UpdateStatus(directive_name, tokenizer, &object_src_status))
      continue;
  }

  if (script_src_status.seen_in_policy && !script_src_status.is_secure)
    return false;

  if (object_src_status.seen_in_policy && !object_src_status.is_secure)
    return false;

  if (default_src_status.seen_in_policy && !default_src_status.is_secure) {
    return script_src_status.seen_in_policy &&
           object_src_status.seen_in_policy;
  }

  return default_src_status.seen_in_policy ||
      (script_src_status.seen_in_policy && object_src_status.seen_in_policy);
}

}  // csp_validator

}  // extensions
