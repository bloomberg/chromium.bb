// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Patterns used in content setting rules.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PATTERN_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PATTERN_H_
#pragma once

#include <ostream>
#include <string>

class GURL;

// A pattern used in content setting rules. See |IsValid| for a description of
// possible patterns.
class ContentSettingsPattern {
 public:
  // Returns a pattern that matches the host of this URL and all subdomains.
  static ContentSettingsPattern FromURL(const GURL& url);

  // Returns a pattern that matches exactly this URL.
  static ContentSettingsPattern FromURLNoWildcard(const GURL& url);

  ContentSettingsPattern() {}

  explicit ContentSettingsPattern(const std::string& pattern)
      : pattern_(pattern) {}

  // True if this is a valid pattern. Valid patterns are
  //   - [*.]domain.tld (matches domain.tld and all sub-domains)
  //   - host (matches an exact hostname)
  //   - a.b.c.d (matches an exact IPv4 ip)
  //   - [a:b:c:d:e:f:g:h] (matches an exact IPv6 ip)
  // TODO(jochen): should also return true for a complete URL without a host.
  bool IsValid() const;

  // True if |url| matches this pattern.
  bool Matches(const GURL& url) const;

  // Returns a std::string representation of this pattern.
  const std::string& AsString() const { return pattern_; }

  bool operator==(const ContentSettingsPattern& other) const {
    return pattern_ == other.pattern_;
  }

  // Canonicalizes the pattern so that it's ASCII only, either
  // in original (if it was already ASCII) or punycode form.
  std::string CanonicalizePattern() const;

  // The version of the pattern format implemented.
  static const int kContentSettingsPatternVersion;

  // The format of a domain wildcard.
  static const char* kDomainWildcard;

  // The length of kDomainWildcard (without the trailing '\0').
  static const size_t kDomainWildcardLength;

 private:
  std::string pattern_;
};

// Stream operator so ContentSettingsPattern can be used in assertion
// statements.
inline std::ostream& operator<<(
    std::ostream& out, const ContentSettingsPattern& pattern) {
  return out << pattern.AsString();
}

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PATTERN_H_
