// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/csp_validator.h"

#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"

namespace extensions {

namespace csp_validator {

namespace {

const char kDefaultSrc[] = "default-src";
const char kScriptSrc[] = "script-src";
const char kObjectSrc[] = "object-src";

const char kSandboxDirectiveName[] = "sandbox";
const char kAllowSameOriginToken[] = "allow-same-origin";
const char kAllowTopNavigation[] = "allow-top-navigation";
const char kAllowPopups[] = "allow-popups";

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

bool HasOnlySecureTokens(base::StringTokenizer& tokenizer,
                         Manifest::Type type) {
  while (tokenizer.GetNext()) {
    std::string source = tokenizer.token();
    StringToLowerASCII(&source);

    // Don't alow whitelisting of all hosts. This boils down to:
    //   1. Maximum of 2 '*' characters.
    //   2. Each '*' is either followed by a '.' or preceded by a ':'
    int wildcards = 0;
    size_t length = source.length();
    for (size_t i = 0; i < length; ++i) {
      if (source[i] == L'*') {
        wildcards++;
        if (wildcards > 2)
          return false;

        bool isWildcardPort = i > 0 && source[i - 1] == L':';
        bool isWildcardSubdomain = i + 1 < length && source[i + 1] == L'.';
        if (!isWildcardPort && !isWildcardSubdomain)
          return false;
      }
    }

    // We might need to relax this whitelist over time.
    if (source == "'self'" ||
        source == "'none'" ||
        source == "http://127.0.0.1" ||
        LowerCaseEqualsASCII(source, "blob:") ||
        LowerCaseEqualsASCII(source, "filesystem:") ||
        LowerCaseEqualsASCII(source, "http://localhost") ||
        StartsWithASCII(source, "http://127.0.0.1:", false) ||
        StartsWithASCII(source, "http://localhost:", false) ||
        StartsWithASCII(source, "https://", true) ||
        StartsWithASCII(source, "chrome://", true) ||
        StartsWithASCII(source, "chrome-extension://", true) ||
        StartsWithASCII(source, "chrome-extension-resource:", true)) {
      continue;
    }

    // crbug.com/146487
    if (type == Manifest::TYPE_EXTENSION ||
        type == Manifest::TYPE_LEGACY_PACKAGED_APP) {
      if (source == "'unsafe-eval'")
        continue;
    }

    return false;
  }

  return true;  // Empty values default to 'none', which is secure.
}

// Returns true if |directive_name| matches |status.directive_name|.
bool UpdateStatus(const std::string& directive_name,
                  base::StringTokenizer& tokenizer,
                  DirectiveStatus* status,
                  Manifest::Type type) {
  if (status->seen_in_policy)
    return false;
  if (directive_name != status->directive_name)
    return false;
  status->seen_in_policy = true;
  status->is_secure = HasOnlySecureTokens(tokenizer, type);
  return true;
}

}  //  namespace

bool ContentSecurityPolicyIsLegal(const std::string& policy) {
  // We block these characters to prevent HTTP header injection when
  // representing the content security policy as an HTTP header.
  const char kBadChars[] = {',', '\r', '\n', '\0'};

  return policy.find_first_of(kBadChars, 0, arraysize(kBadChars)) ==
      std::string::npos;
}

bool ContentSecurityPolicyIsSecure(const std::string& policy,
                                   Manifest::Type type) {
  // See http://www.w3.org/TR/CSP/#parse-a-csp-policy for parsing algorithm.
  std::vector<std::string> directives;
  base::SplitString(policy, ';', &directives);

  DirectiveStatus default_src_status(kDefaultSrc);
  DirectiveStatus script_src_status(kScriptSrc);
  DirectiveStatus object_src_status(kObjectSrc);

  for (size_t i = 0; i < directives.size(); ++i) {
    std::string& input = directives[i];
    base::StringTokenizer tokenizer(input, " \t\r\n");
    if (!tokenizer.GetNext())
      continue;

    std::string directive_name = tokenizer.token();
    StringToLowerASCII(&directive_name);

    if (UpdateStatus(directive_name, tokenizer, &default_src_status, type))
      continue;
    if (UpdateStatus(directive_name, tokenizer, &script_src_status, type))
      continue;
    if (UpdateStatus(directive_name, tokenizer, &object_src_status, type))
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

bool ContentSecurityPolicyIsSandboxed(
    const std::string& policy, Manifest::Type type) {
  // See http://www.w3.org/TR/CSP/#parse-a-csp-policy for parsing algorithm.
  std::vector<std::string> directives;
  base::SplitString(policy, ';', &directives);

  bool seen_sandbox = false;

  for (size_t i = 0; i < directives.size(); ++i) {
    std::string& input = directives[i];
    base::StringTokenizer tokenizer(input, " \t\r\n");
    if (!tokenizer.GetNext())
      continue;

    std::string directive_name = tokenizer.token();
    StringToLowerASCII(&directive_name);

    if (directive_name != kSandboxDirectiveName)
      continue;

    seen_sandbox = true;

    while (tokenizer.GetNext()) {
      std::string token = tokenizer.token();
      StringToLowerASCII(&token);

      // The same origin token negates the sandboxing.
      if (token == kAllowSameOriginToken)
        return false;

      // Platform apps don't allow navigation.
      if (type == Manifest::TYPE_PLATFORM_APP) {
        if (token == kAllowTopNavigation)
          return false;
      }
    }
  }

  return seen_sandbox;
}

}  // csp_validator

}  // extensions
