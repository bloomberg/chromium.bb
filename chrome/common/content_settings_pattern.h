// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Patterns used in content setting rules.

#ifndef CHROME_COMMON_CONTENT_SETTINGS_PATTERN_H_
#define CHROME_COMMON_CONTENT_SETTINGS_PATTERN_H_
#pragma once

#include <ostream>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"

class GURL;

namespace content_settings {
class PatternParser;
}

namespace IPC {
class Message;
}

// A pattern used in content setting rules. See |IsValid| for a description of
// possible patterns.
class ContentSettingsPattern {
 public:
  // Each content settings pattern describes a set of origins. Patterns, and the
  // sets they describe, have specific relations. |Relation| describes the
  // relation of two patterns A and B. When pattern A is compared with pattern B
  // (A compare B) interesting relations are:
  // - IDENTITY:
  //   Pattern A and B are identical. The patterns are equal.
  //
  // - DISJOINT_ORDER_PRE:
  //   Pattern A and B have no intersection. A and B never match the origin of
  //   a URL at the same time. But pattern A has a higher precedence than
  //   pattern B when patterns are sorted.
  //
  // - DISJOINT_ORDER_POST:
  //   Pattern A and B have no intersection. A and B never match the origin of
  //   a URL at the same time. But pattern A has a lower precedence than
  //   pattern B when patterns are sorted.
  //
  // - SUCCESSOR:
  //   Pattern A and B have an intersection. But pattern B has a higher
  //   precedence than pattern A for URLs that are matched by both pattern.
  //
  // - PREDECESSOR:
  //   Pattern A and B have an intersection. But pattern A has a higher
  //   precedence than pattern B for URLs that are matched by both pattern.
  enum Relation {
    DISJOINT_ORDER_POST = -2,
    SUCCESSOR = -1,
    IDENTITY = 0,
    PREDECESSOR = 1,
    DISJOINT_ORDER_PRE = 2,
  };

  struct PatternParts {
    PatternParts();
    ~PatternParts();

    // Lowercase string of the URL scheme to match. This string is empty if the
    // |is_scheme_wildcard| flag is set.
    std::string scheme;

    // True if the scheme wildcard is set.
    bool is_scheme_wildcard;

    // Normalized string that is either of the following:
    // - IPv4 or IPv6
    // - hostname
    // - domain
    // - empty string if the |is_host_wildcard flag is set.
    std::string host;

    // True if the domain wildcard is set.
    bool has_domain_wildcard;

    // String with the port to match. This string is empty if the
    // |is_port_wildcard| flag is set.
    std::string port;

    // True if the port wildcard is set.
    bool is_port_wildcard;

    // TODO(markusheintz): Needed for legacy reasons. Remove. Path
    // specification. Only used for content settings pattern with a "file"
    // scheme part.
    std::string path;
  };

  class BuilderInterface {
   public:
    virtual ~BuilderInterface() {}

    virtual BuilderInterface* WithPort(const std::string& port) = 0;

    virtual BuilderInterface* WithPortWildcard() = 0;

    virtual BuilderInterface* WithHost(const std::string& host) = 0;

    virtual BuilderInterface* WithDomainWildcard() = 0;

    virtual BuilderInterface* WithScheme(const std::string& scheme) = 0;

    virtual BuilderInterface* WithSchemeWildcard() = 0;

    virtual BuilderInterface* WithPath(const std::string& path) = 0;

    virtual BuilderInterface* Invalid() = 0;

    // Returns a content settings pattern according to the current configuration
    // of the builder.
    virtual ContentSettingsPattern Build() = 0;
  };

  static BuilderInterface* CreateBuilder(bool use_legacy_validate);

  // The version of the pattern format implemented.
  static const int kContentSettingsPatternVersion;

  // The format of a domain wildcard.
  static const char* kDomainWildcard;

  // The length of kDomainWildcard (without the trailing '\0').
  static const size_t kDomainWildcardLength;

  // Returns a wildcard content settings pattern that matches all possible valid
  // origins.
  static ContentSettingsPattern Wildcard();

  // Returns a pattern that matches the scheme and host of this URL, as well as
  // all subdomains and ports.
  static ContentSettingsPattern FromURL(const GURL& url);

  // Returns a pattern that matches exactly this URL.
  static ContentSettingsPattern FromURLNoWildcard(const GURL& url);

  // Returns a pattern that matches the given pattern specification.
  // Valid patterns specifications are:
  //   - [*.]domain.tld (matches domain.tld and all sub-domains)
  //   - host (matches an exact hostname)
  //   - scheme://host:port (supported schemes: http,https)
  //   - scheme://[*.]domain.tld:port (supported schemes: http,https)
  //   - file://path (The path has to be an absolute path and start with a '/')
  //   - a.b.c.d (matches an exact IPv4 ip)
  //   - [a:b:c:d:e:f:g:h] (matches an exact IPv6 ip)
  static ContentSettingsPattern FromString(const std::string& pattern_spec);

  static ContentSettingsPattern LegacyFromString(
      const std::string& pattern_spec);

  // Constructs an empty pattern. Empty patterns are invalid patterns. Invalid
  // patterns match nothing.
  ContentSettingsPattern();

  // Serializes the pattern to an IPC message or deserializes it.
  void WriteToMessage(IPC::Message* m) const;
  bool ReadFromMessage(const IPC::Message* m, void** iter);

  // True if this is a valid pattern.
  bool IsValid() const { return is_valid_; }

  // True if |url| matches this pattern.
  bool Matches(const GURL& url) const;

  // Returns a std::string representation of this pattern.
  const std::string ToString() const;

  // Compares the pattern with a given |other| pattern and returns the
  // |Relation| of the two patterns.
  Relation Compare(const ContentSettingsPattern& other) const;

  // Returns true if the pattern and the |other| pattern are identical.
  bool operator==(const ContentSettingsPattern& other) const;

  // Returns true if the pattern and the |other| pattern are not identical.
  bool operator!=(const ContentSettingsPattern& other) const;

  // Returns true if the pattern has a lower priority than the |other| pattern.
  bool operator<(const ContentSettingsPattern& other) const;

  // Returns true if the pattern has a higher priority than the |other| pattern.
  bool operator>(const ContentSettingsPattern& other) const;

 private:
  friend class content_settings::PatternParser;
  friend class Builder;
  FRIEND_TEST_ALL_PREFIXES(ContentSettingsPatternParserTest, SerializePatterns);

  class Builder : public BuilderInterface {
    public:
     explicit Builder(bool use_legacy_validate);
     virtual ~Builder();

     // Overrides BuilderInterface
     virtual BuilderInterface* WithPort(const std::string& port) OVERRIDE;

     virtual BuilderInterface* WithPortWildcard() OVERRIDE;

     virtual BuilderInterface* WithHost(const std::string& host) OVERRIDE;

     virtual BuilderInterface* WithDomainWildcard() OVERRIDE;

     virtual BuilderInterface* WithScheme(const std::string& scheme) OVERRIDE;

     virtual BuilderInterface* WithSchemeWildcard() OVERRIDE;

     virtual BuilderInterface* WithPath(const std::string& path) OVERRIDE;

     virtual BuilderInterface* Invalid() OVERRIDE;

     virtual ContentSettingsPattern Build() OVERRIDE;
    private:
     // Canonicalizes the pattern parts so that they are ASCII only, either
     // in original (if it was already ASCII) or punycode form.
     static void Canonicalize(PatternParts* parts);

     // Returns true when the pattern |parts| represent a valid pattern.
     static bool Validate(const PatternParts& parts);

     static bool LegacyValidate(const PatternParts& parts);

     bool is_valid_;

     bool use_legacy_validate_;

     PatternParts parts_;

     DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  static Relation CompareScheme(
      const ContentSettingsPattern::PatternParts& parts,
      const ContentSettingsPattern::PatternParts& other_parts);

  static Relation CompareHost(
      const ContentSettingsPattern::PatternParts& parts,
      const ContentSettingsPattern::PatternParts& other_parts);

  static Relation ComparePort(
      const ContentSettingsPattern::PatternParts& parts,
      const ContentSettingsPattern::PatternParts& other_parts);

  static bool Validate(const PatternParts& parts);

  ContentSettingsPattern(const PatternParts& parts, bool valid);

  PatternParts parts_;

  bool is_valid_;
};

// Stream operator so ContentSettingsPattern can be used in assertion
// statements.
inline std::ostream& operator<<(
    std::ostream& out, const ContentSettingsPattern& pattern) {
  return out << pattern.ToString();
}

#endif  // CHROME_COMMON_CONTENT_SETTINGS_PATTERN_H_
