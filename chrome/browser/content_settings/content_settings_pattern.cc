// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_pattern.h"

#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"

namespace {
bool IsValidHostlessPattern(const std::string& pattern) {
  std::string file_scheme_plus_separator(chrome::kFileScheme);
  file_scheme_plus_separator += chrome::kStandardSchemeSeparator;

  return StartsWithASCII(pattern, file_scheme_plus_separator, false);
}
}  // namespace

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
const char* ContentSettingsPattern::kDomainWildcard = "[*.]";
const size_t ContentSettingsPattern::kDomainWildcardLength = 4;

// static
ContentSettingsPattern ContentSettingsPattern::FromURL(
    const GURL& url) {
  return ContentSettingsPattern(!url.has_host() || url.HostIsIPAddress() ?
      net::GetHostOrSpecFromURL(url) :
      std::string(kDomainWildcard) + url.host());
}

// static
ContentSettingsPattern ContentSettingsPattern::FromURLNoWildcard(
    const GURL& url) {
  return ContentSettingsPattern(net::GetHostOrSpecFromURL(url));
}

bool ContentSettingsPattern::IsValid() const {
  if (pattern_.empty())
    return false;

  if (IsValidHostlessPattern(pattern_))
    return true;

  const std::string host(pattern_.length() > kDomainWildcardLength &&
                         StartsWithASCII(pattern_, kDomainWildcard, false) ?
                         pattern_.substr(kDomainWildcardLength) :
                         pattern_);
  url_canon::CanonHostInfo host_info;
  return host.find('*') == std::string::npos &&
         !net::CanonicalizeHost(host, &host_info).empty();
}

bool ContentSettingsPattern::Matches(const GURL& url) const {
  if (!IsValid())
    return false;

  const std::string host(net::GetHostOrSpecFromURL(url));
  if (pattern_.length() < kDomainWildcardLength ||
      !StartsWithASCII(pattern_, kDomainWildcard, false))
    return pattern_ == host;

  const size_t match =
      host.rfind(pattern_.substr(kDomainWildcardLength));

  return (match != std::string::npos) &&
         (match == 0 || host[match - 1] == '.') &&
         (match + pattern_.length() - kDomainWildcardLength == host.length());
}

std::string ContentSettingsPattern::CanonicalizePattern() const {
  if (!IsValid())
    return "";

  if (IsValidHostlessPattern(pattern_))
    return GURL(pattern_).spec();

  bool starts_with_wildcard = pattern_.length() > kDomainWildcardLength &&
      StartsWithASCII(pattern_, kDomainWildcard, false);

  const std::string host(starts_with_wildcard ?
      pattern_.substr(kDomainWildcardLength) : pattern_);

  std::string canonicalized_pattern =
      starts_with_wildcard ? kDomainWildcard : "";

  url_canon::CanonHostInfo host_info;
  canonicalized_pattern += net::CanonicalizeHost(host, &host_info);

  return canonicalized_pattern;
}
