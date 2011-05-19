// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_pattern.h"

#include "base/string_util.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/content_settings/content_settings_pattern_parser.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"

namespace {

std::string GetDefaultPort(const std::string& scheme) {
  if (scheme == chrome::kHttpScheme)
    return "80";
  if (scheme == chrome::kHttpsScheme)
    return "443";
  return "";
}

// Returns true if |sub_domain| is a sub domain or equls |domain|.  E.g.
// "mail.google.com" is a sub domain of "google.com" but "evilhost.com" is not a
// subdomain of "host.com".
bool IsSubDomainOrEqual(const std::string sub_domain,
                        const std::string domain) {
  // The empty string serves as wildcard. Each domain is a subdomain of the
  // wildcard.
  if (domain.empty())
    return true;
  const size_t match = sub_domain.rfind(domain);
  if (match == std::string::npos ||
      (match > 0 && sub_domain[match - 1] != '.') ||
      (match + domain.length() != sub_domain.length())) {
    return false;
  }
  return true;
}

typedef ContentSettingsPattern::BuilderInterface BuilderInterface;

}  // namespace

// ////////////////////////////////////////////////////////////////////////////
// ContentSettingsPattern::Builder
//

ContentSettingsPattern::Builder::Builder() : invalid_(false) {}

ContentSettingsPattern::Builder::~Builder() {}

BuilderInterface* ContentSettingsPattern::Builder::WithPort(
    const std::string& port) {
  parts_.port = port;
  parts_.is_port_wildcard = false;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithPortWildcard() {
  parts_.port = "";
  parts_.is_port_wildcard = true;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithHost(
    const std::string& host) {
  parts_.host = host;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithDomainWildcard() {
  parts_.has_domain_wildcard = true;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithScheme(
    const std::string& scheme) {
  parts_.scheme = scheme;
  parts_.is_scheme_wildcard = false;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithSchemeWildcard() {
  parts_.scheme = "";
  parts_.is_scheme_wildcard = true;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::WithPath(
    const std::string& path) {
  parts_.path = path;
  return this;
}

BuilderInterface* ContentSettingsPattern::Builder::Invalid() {
  invalid_ = true;
  return this;
}

ContentSettingsPattern ContentSettingsPattern::Builder::Build() {
  if (invalid_)
    return ContentSettingsPattern();
  Canonicalize(&parts_);
  return ContentSettingsPattern(parts_);
}

// static
void ContentSettingsPattern::Builder::Canonicalize(PatternParts* parts) {
  // Canonicalize the scheme part.
  const std::string scheme(StringToLowerASCII(parts->scheme));
  parts->scheme = scheme;

  if (parts->scheme == std::string(chrome::kFileScheme)) {
    GURL url(std::string(chrome::kFileScheme) +
             std::string(chrome::kStandardSchemeSeparator) + parts->path);
    parts->path = url.path();
  }

  // Canonicalize the host part.
  const std::string host(parts->host);
  url_canon::CanonHostInfo host_info;
  const std::string canonicalized_host(net::CanonicalizeHost(host, &host_info));

  parts->host = "";
  if ((host.find('*') == std::string::npos) &&
      !canonicalized_host.empty()) {
    // Valid host.
    parts->host += canonicalized_host;
  }
}

// ////////////////////////////////////////////////////////////////////////////
// ContentSettingsPattern::PatternParts
//
ContentSettingsPattern::PatternParts::PatternParts()
        : is_scheme_wildcard(false),
          has_domain_wildcard(false),
          is_port_wildcard(false) {}

ContentSettingsPattern::PatternParts::~PatternParts() {}

// ////////////////////////////////////////////////////////////////////////////
// ContentSettingsPattern
//

// The version of the pattern format implemented. Version 1 includes the
// following patterns:
//   - [*.]domain.tld (matches domain.tld and all sub-domains)
//   - host (matches an exact hostname)
//   - a.b.c.d (matches an exact IPv4 ip)
//   - [a:b:c:d:e:f:g:h] (matches an exact IPv6 ip)
//   - file:///tmp/test.html (a complete URL without a host)
// Version 2 adds a resource identifier for plugins.
// TODO(jochen): update once this feature is no longer behind a flag.
const int ContentSettingsPattern::kContentSettingsPatternVersion = 1;

// TODO(markusheintz): These two constants were moved to the Pattern Parser.
// Remove once the dependency of the ContentSettingsBaseProvider is removed.
const char* ContentSettingsPattern::kDomainWildcard = "[*.]";
const size_t ContentSettingsPattern::kDomainWildcardLength = 4;

// static
BuilderInterface* ContentSettingsPattern::CreateBuilder() {
  return new Builder();
}

// static
ContentSettingsPattern ContentSettingsPattern::FromURL(
    const GURL& url) {
  scoped_ptr<ContentSettingsPattern::BuilderInterface> builder(
      ContentSettingsPattern::CreateBuilder());

  if (url.SchemeIsFile()) {
    builder->WithScheme(url.scheme())->WithPath(url.path());
  } else {
    // Please keep the order of the ifs below as URLs with an IP as host can
    // also have a "http" scheme.
    if (url.HostIsIPAddress()) {
      builder->WithScheme(url.scheme())->WithHost(url.host());
    } else if (url.SchemeIs(chrome::kHttpScheme)) {
      builder->WithSchemeWildcard()->WithDomainWildcard()->WithHost(url.host());
    } else if (url.SchemeIs(chrome::kHttpsScheme)) {
      builder->WithScheme(url.scheme())->WithDomainWildcard()->WithHost(
          url.host());
    } else {
      // Unsupported scheme
    }
    if (url.port().empty()) {
      builder->WithPortWildcard();
    } else {
      builder->WithPort(url.port());
    }
  }
  return builder->Build();
}

// static
ContentSettingsPattern ContentSettingsPattern::FromURLNoWildcard(
    const GURL& url) {
  scoped_ptr<ContentSettingsPattern::BuilderInterface> builder(
      ContentSettingsPattern::CreateBuilder());

  if (url.SchemeIsFile()) {
    builder->WithScheme(url.scheme())->WithPath(url.path());
  } else {
    builder->WithScheme(url.scheme())->WithHost(url.host());
    if (url.port().empty()) {
      builder->WithPort(GetDefaultPort(url.scheme()));
    } else {
      builder->WithPort(url.port());
    }
  }
  return builder->Build();
}

// static
ContentSettingsPattern ContentSettingsPattern::FromString(
    const std::string& pattern_spec) {
  scoped_ptr<ContentSettingsPattern::BuilderInterface> builder(
      ContentSettingsPattern::CreateBuilder());
  content_settings::PatternParser::Parse(pattern_spec, builder.get());
  return builder->Build();
}

// static
ContentSettingsPattern ContentSettingsPattern::Wildcard() {
  scoped_ptr<ContentSettingsPattern::BuilderInterface> builder(
      ContentSettingsPattern::CreateBuilder());
  builder->WithSchemeWildcard()->WithDomainWildcard()->WithPortWildcard();
  return builder->Build();
}

// static
bool ContentSettingsPattern::Validate(const PatternParts& parts) {
  // If the pattern is for a "file-pattern" test if it is valid.
  if (parts.scheme == std::string(chrome::kFileScheme) &&
      !parts.is_scheme_wildcard &&
      parts.host.empty() &&
      parts.port.empty())
    return true;

  // If the pattern is for an extension URL test if it is valid.
  if (parts.scheme == std::string(chrome::kExtensionScheme) &&
      !parts.is_scheme_wildcard &&
      !parts.host.empty() &&
      !parts.has_domain_wildcard &&
      parts.port.empty() &&
      !parts.is_port_wildcard)
    return true;

  // Non-file patterns are invalid if either the scheme, host or port part is
  // empty.
  if ((parts.scheme.empty() && !parts.is_scheme_wildcard) ||
      (parts.host.empty() && !parts.has_domain_wildcard) ||
      (parts.port.empty() && !parts.is_port_wildcard))
    return false;

  // Test if the scheme is supported or a wildcard.
  if (!parts.is_scheme_wildcard &&
      parts.scheme != std::string(chrome::kHttpScheme) &&
      parts.scheme != std::string(chrome::kHttpsScheme)) {
    return false;
  }
  return true;
}

ContentSettingsPattern::ContentSettingsPattern()
  : is_valid_(false) {
}

ContentSettingsPattern::ContentSettingsPattern(const PatternParts& parts)
    : parts_(parts) {
  is_valid_ = Validate(parts);
}

bool ContentSettingsPattern::Matches(
    const GURL& url) const {
  // An invalid pattern matches nothing.
  if (!is_valid_)
    return false;

  // Match the scheme part.
  const std::string scheme(url.scheme());
  if (!parts_.is_scheme_wildcard &&
      parts_.scheme != scheme) {
    return false;
  }

  // File URLs have no host. For file URLs check if the url path matches the
  // path in the pattern.
  // TODO(markusheintz): This should change in the future. There should be only
  // one setting for all file URLs. So the path should be ignored.
  if (!parts_.is_scheme_wildcard &&
      scheme == std::string(chrome::kFileScheme)) {
    if (parts_.path == std::string(url.path()))
      return true;
    return false;
  }

  // Match the host part.
  const std::string host(url.host());
  if (!parts_.has_domain_wildcard) {
    if (parts_.host != host)
      return false;
  } else {
    if (!IsSubDomainOrEqual(host, parts_.host))
      return false;
  }

  // For chrome extensions URLs ignore the port.
  if (parts_.scheme == std::string(chrome::kExtensionScheme))
    return true;

  // Match the port part.
  std::string port(url.port());

  // Use the default port if the port string is empty. GURL returns an empty
  // string if no port at all was specified or if the default port was
  // specified.
  if (port.empty()) {
    port = GetDefaultPort(scheme);
  }

  if (!parts_.is_port_wildcard &&
      parts_.port != port ) {
    return false;
  }

  return true;
}

const std::string ContentSettingsPattern::ToString() const {
  if (IsValid())
    return content_settings::PatternParser::ToString(parts_);
  else
    return "";
}

ContentSettingsPattern::Relation ContentSettingsPattern::Compare(
    const ContentSettingsPattern& other) const {
  if (this == &other)
    return IDENTITY;

  if (!is_valid_ || !other.is_valid_) {
    NOTREACHED();
    return DISJOINT;
  }

  // If either host, port or scheme are disjoint return immediately.
  Relation host_relation = CompareHost(parts_, other.parts_);
  if (host_relation == DISJOINT)
    return DISJOINT;
  Relation port_relation = ComparePort(parts_, other.parts_);
  if (port_relation == DISJOINT)
    return DISJOINT;
  Relation scheme_relation = CompareScheme(parts_, other.parts_);
  if (scheme_relation == DISJOINT)
    return DISJOINT;

  if (host_relation != IDENTITY)
    return host_relation;
  if (port_relation != IDENTITY)
    return port_relation;
  return scheme_relation;
}

// static
ContentSettingsPattern::Relation ContentSettingsPattern::CompareHost(
    const ContentSettingsPattern::PatternParts& parts,
    const ContentSettingsPattern::PatternParts& other_parts) {
  if (!parts.has_domain_wildcard && !other_parts.has_domain_wildcard) {
    // Case 1: No host starts with a wild card
    if (parts.host == other_parts.host) {
      return ContentSettingsPattern::IDENTITY;
    } else {
      return ContentSettingsPattern::DISJOINT;
    }
  } else if (parts.has_domain_wildcard && !other_parts.has_domain_wildcard) {
    // Case 2: |host| starts with a domain wildcard and |other_host| does not
    // start with a domain wildcard.
    // Examples:
    // "this" host:   [*.]google.com
    // "other" host:  google.com
    //
    // [*.]google.com
    // mail.google.com
    //
    // [*.]mail.google.com
    // google.com
    //
    // [*.]youtube.com
    // google.de
    //
    // [*.]youtube.com
    // mail.google.com
    //
    // *
    // google.de
    if (IsSubDomainOrEqual(other_parts.host, parts.host)) {
      return ContentSettingsPattern::SUCCESSOR;
    } else {
      return ContentSettingsPattern::DISJOINT;
    }
  } else if (!parts.has_domain_wildcard && other_parts.has_domain_wildcard) {
    // Case 3: |host| starts NOT with a domain wildcard and |other_host| starts
    // with a domain wildcard.
    if (IsSubDomainOrEqual(parts.host, other_parts.host)) {
      return ContentSettingsPattern::PREDECESSOR;
    } else {
      return ContentSettingsPattern::DISJOINT;
    }
  } else if (parts.has_domain_wildcard && other_parts.has_domain_wildcard) {
    // Case 4: |host| and |other_host| both start with a domain wildcard.
    // Examples:
    // [*.]google.com
    // [*.]google.com
    //
    // [*.]google.com
    // [*.]mail.google.com
    //
    // [*.]youtube.com
    // [*.]google.de
    //
    // [*.]youtube.com
    // [*.]mail.google.com
    //
    // [*.]youtube.com
    // *
    //
    // *
    // [*.]youtube.com
    if (parts.host == other_parts.host) {
      return ContentSettingsPattern::IDENTITY;
    } else if (IsSubDomainOrEqual(other_parts.host, parts.host)) {
      return ContentSettingsPattern::SUCCESSOR;
    } else if (IsSubDomainOrEqual(parts.host, other_parts.host)) {
      return ContentSettingsPattern::PREDECESSOR;
    } else {
      return ContentSettingsPattern::DISJOINT;
    }
  }

  NOTREACHED();
  return ContentSettingsPattern::IDENTITY;
}

// static
ContentSettingsPattern::Relation ContentSettingsPattern::CompareScheme(
    const ContentSettingsPattern::PatternParts& parts,
    const ContentSettingsPattern::PatternParts& other_parts) {
  if (parts.is_scheme_wildcard && !other_parts.is_scheme_wildcard)
    return ContentSettingsPattern::SUCCESSOR;
  if (!parts.is_scheme_wildcard && other_parts.is_scheme_wildcard)
    return ContentSettingsPattern::PREDECESSOR;
  if (parts.scheme != other_parts.scheme)
    return ContentSettingsPattern::DISJOINT;
  return ContentSettingsPattern::IDENTITY;
}

// static
ContentSettingsPattern::Relation ContentSettingsPattern::ComparePort(
    const ContentSettingsPattern::PatternParts& parts,
    const ContentSettingsPattern::PatternParts& other_parts) {
  if (parts.is_port_wildcard && !other_parts.is_port_wildcard)
    return ContentSettingsPattern::SUCCESSOR;
  if (!parts.is_port_wildcard && other_parts.is_port_wildcard)
    return ContentSettingsPattern::PREDECESSOR;
  if (parts.port != other_parts.port)
    return ContentSettingsPattern::DISJOINT;
  return ContentSettingsPattern::IDENTITY;
}
