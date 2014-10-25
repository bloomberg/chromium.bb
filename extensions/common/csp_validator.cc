// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/csp_validator.h"

#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace extensions {

namespace csp_validator {

namespace {

const char kDefaultSrc[] = "default-src";
const char kScriptSrc[] = "script-src";
const char kObjectSrc[] = "object-src";

const char kSandboxDirectiveName[] = "sandbox";
const char kAllowSameOriginToken[] = "allow-same-origin";
const char kAllowTopNavigation[] = "allow-top-navigation";

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

// Returns whether |url| starts with |scheme_and_separator| and does not have a
// too permissive wildcard host name. If |should_check_rcd| is true, then the
// Public suffix list is used to exclude wildcard TLDs such as "https://*.org".
bool isNonWildcardTLD(const std::string& url,
                      const std::string& scheme_and_separator,
                      bool should_check_rcd) {
  if (!StartsWithASCII(url, scheme_and_separator, true))
    return false;

  size_t start_of_host = scheme_and_separator.length();

  size_t end_of_host = url.find("/", start_of_host);
  if (end_of_host == std::string::npos)
    end_of_host = url.size();

  // Note: It is sufficient to only compare the first character against '*'
  // because the CSP only allows wildcards at the start of a directive, see
  // host-source and host-part at http://www.w3.org/TR/CSP2/#source-list-syntax
  bool is_wildcard_subdomain = end_of_host > start_of_host + 2 &&
      url[start_of_host] == '*' && url[start_of_host + 1] == '.';
  if (is_wildcard_subdomain)
    start_of_host += 2;

  size_t start_of_port = url.rfind(":", end_of_host);
  // The ":" check at the end of the following condition is used to avoid
  // treating the last part of an IPv6 address as a port.
  if (start_of_port > start_of_host && url[start_of_port - 1] != ':') {
    bool is_valid_port = false;
    // Do a quick sanity check. The following check could mistakenly flag
    // ":123456" or ":****" as valid, but that does not matter because the
    // relaxing CSP directive will just be ignored by Blink.
    for (size_t i = start_of_port + 1; i < end_of_host; ++i) {
      is_valid_port = IsAsciiDigit(url[i]) || url[i] == '*';
      if (!is_valid_port)
        break;
    }
    if (is_valid_port)
      end_of_host = start_of_port;
  }

  std::string host(url, start_of_host, end_of_host - start_of_host);
  // Global wildcards are not allowed.
  if (host.empty() || host.find("*") != std::string::npos)
    return false;

  if (!is_wildcard_subdomain || !should_check_rcd)
    return true;

  // Allow *.googleapis.com to be whitelisted for backwards-compatibility.
  // (crbug.com/409952)
  if (host == "googleapis.com")
    return true;

  // Wildcards on subdomains of a TLD are not allowed.
  size_t registry_length = net::registry_controlled_domains::GetRegistryLength(
      host,
      net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return registry_length != 0;
}

bool HasOnlySecureTokens(base::StringTokenizer& tokenizer,
                         Manifest::Type type) {
  while (tokenizer.GetNext()) {
    std::string source = tokenizer.token();
    base::StringToLowerASCII(&source);

    // We might need to relax this whitelist over time.
    if (source == "'self'" ||
        source == "'none'" ||
        source == "http://127.0.0.1" ||
        LowerCaseEqualsASCII(source, "blob:") ||
        LowerCaseEqualsASCII(source, "filesystem:") ||
        LowerCaseEqualsASCII(source, "http://localhost") ||
        StartsWithASCII(source, "http://127.0.0.1:", true) ||
        StartsWithASCII(source, "http://localhost:", true) ||
        isNonWildcardTLD(source, "https://", true) ||
        isNonWildcardTLD(source, "chrome://", false) ||
        isNonWildcardTLD(source,
                         std::string(extensions::kExtensionScheme) +
                         url::kStandardSchemeSeparator,
                         false) ||
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
    base::StringToLowerASCII(&directive_name);

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
    base::StringToLowerASCII(&directive_name);

    if (directive_name != kSandboxDirectiveName)
      continue;

    seen_sandbox = true;

    while (tokenizer.GetNext()) {
      std::string token = tokenizer.token();
      base::StringToLowerASCII(&token);

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

}  // namespace csp_validator

}  // namespace extensions
