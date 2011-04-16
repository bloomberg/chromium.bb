// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_COMMON_EXTENSIONS_URL_PATTERN_H_
#define CHROME_COMMON_EXTENSIONS_URL_PATTERN_H_
#pragma once

#include <functional>
#include <string>
#include <vector>

class GURL;

// A pattern that can be used to match URLs. A URLPattern is a very restricted
// subset of URL syntax:
//
// <url-pattern> := <scheme>://<host><path> | '<all_urls>'
// <scheme> := '*' | 'http' | 'https' | 'file' | 'ftp' | 'chrome'
// <host> := '*' | '*.' <anychar except '/' and '*'>+
// <path> := '/' <any chars>
//
// * Host is not used when the scheme is 'file'.
// * The path can have embedded '*' characters which act as glob wildcards.
// * '<all_urls>' is a special pattern that matches any URL that contains a
//   valid scheme (as specified by valid_schemes_).
// * The '*' scheme pattern excludes file URLs.
//
// Examples of valid patterns:
// - http://*/*
// - http://*/foo*
// - https://*.google.com/foo*bar
// - file://monkey*
// - http://127.0.0.1/*
//
// Examples of invalid patterns:
// - http://* -- path not specified
// - http://*foo/bar -- * not allowed as substring of host component
// - http://foo.*.bar/baz -- * must be first component
// - http:/bar -- scheme separator not found
// - foo://* -- invalid scheme
// - chrome:// -- we don't support chrome internal URLs
//
// Design rationale:
// * We need to be able to tell users what 'sites' a given URLPattern will
//   affect. For example "This extension will interact with the site
//   'www.google.com'.
// * We'd like to be able to convert as many existing Greasemonkey @include
//   patterns to URLPatterns as possible. Greasemonkey @include patterns are
//   simple globs, so this won't be perfect.
// * Although we would like to support any scheme, it isn't clear what to tell
//   users about URLPatterns that affect data or javascript URLs, so those are
//   left out for now.
//
// From a 2008-ish crawl of userscripts.org, the following patterns were found
// in @include lines:
// - total lines                    : 24471
// - @include *                     :   919
// - @include http://[^\*]+?/       : 11128 (no star in host)
// - @include http://\*\.[^\*]+?/   :  2325 (host prefixed by *.)
// - @include http://\*[^\.][^\*]+?/:  1524 (host prefixed by *, no dot -- many
//                                           appear to only need subdomain
//                                           matching, not real prefix matching)
// - @include http://[^\*/]+\*/     :   320 (host suffixed by *)
// - @include contains .tld         :   297 (host suffixed by .tld -- a special
//                                           Greasemonkey domain component that
//                                           tries to match all valid registry-
//                                           controlled suffixes)
// - @include http://\*/            :   228 (host is * exactly, but there is
//                                           more to the pattern)
//
// So, we can support at least half of current @include lines without supporting
// subdomain matching. We can pick up at least another 10% by supporting
// subdomain matching. It is probably possible to coerce more of the existing
// patterns to URLPattern, but the resulting pattern will be more restrictive
// than the original glob, which is probably better than nothing.
class URLPattern {
 public:
  // A collection of scheme bitmasks for use with valid_schemes.
  enum SchemeMasks {
    SCHEME_NONE       = 0,
    SCHEME_HTTP       = 1 << 0,
    SCHEME_HTTPS      = 1 << 1,
    SCHEME_FILE       = 1 << 2,
    SCHEME_FTP        = 1 << 3,
    SCHEME_CHROMEUI   = 1 << 4,
    SCHEME_FILESYSTEM = 1 << 5,
    // SCHEME_ALL will match every scheme, including chrome://, chrome-
    // extension://, about:, etc. Because this has lots of security
    // implications, third-party extensions should never be able to get access
    // to URL patterns initialized this way. It should only be used for internal
    // Chrome code.
    SCHEME_ALL      = -1,
  };

  // Options for URLPattern::Parse().
  enum ParseOption {
    PARSE_LENIENT,
    PARSE_STRICT
  };

  // Error codes returned from Parse().
  enum ParseResult {
    PARSE_SUCCESS = 0,
    PARSE_ERROR_MISSING_SCHEME_SEPARATOR,
    PARSE_ERROR_INVALID_SCHEME,
    PARSE_ERROR_WRONG_SCHEME_SEPARATOR,
    PARSE_ERROR_EMPTY_HOST,
    PARSE_ERROR_INVALID_HOST_WILDCARD,
    PARSE_ERROR_EMPTY_PATH,
    PARSE_ERROR_HAS_COLON,  // Only checked when strict checks are enabled.
    NUM_PARSE_RESULTS
  };

  // The <all_urls> string pattern.
  static const char kAllUrlsPattern[];

  // Construct an URLPattern with the given set of allowable schemes. See
  // valid_schemes_ for more info.
  explicit URLPattern(int valid_schemes);

  // Convenience to construct a URLPattern from a string. The string is expected
  // to be a valid pattern. If the string is not known ahead of time, use
  // Parse() instead, which returns success or failure.
  URLPattern(int valid_schemes, const std::string& pattern);

#if defined(_MSC_VER) && _MSC_VER >= 1600
  // Note: don't use this directly. This exists so URLPattern can be used
  // with STL containers.  Starting with Visual Studio 2010, we can't have this
  // method private and use "friend class std::vector<URLPattern>;" as we used
  // to do.
  URLPattern();
#endif

  ~URLPattern();

  // Gets the bitmask of valid schemes.
  int valid_schemes() const { return valid_schemes_; }
  void set_valid_schemes(int valid_schemes) { valid_schemes_ = valid_schemes; }

  // Gets the host the pattern matches. This can be an empty string if the
  // pattern matches all hosts (the input was <scheme>://*/<whatever>).
  const std::string& host() const { return host_; }
  void set_host(const std::string& host) { host_ = host; }

  // Gets whether to match subdomains of host().
  bool match_subdomains() const { return match_subdomains_; }
  void set_match_subdomains(bool val) { match_subdomains_ = val; }

  // Gets the path the pattern matches with the leading slash. This can have
  // embedded asterisks which are interpreted using glob rules.
  const std::string& path() const { return path_; }
  void SetPath(const std::string& path);

  // Returns true if this pattern matches all urls.
  bool match_all_urls() const { return match_all_urls_; }
  void set_match_all_urls(bool val) { match_all_urls_ = val; }

  // Initializes this instance by parsing the provided string. Returns
  // URLPattern::PARSE_SUCCESS on success, or an error code otherwise. On
  // failure, this instance will have some intermediate values and is in an
  // invalid state.  Adding error checks to URLPattern::Parse() can cause
  // patterns in installed extensions to fail.  If an installed extension
  // uses a pattern that was valid but fails a new error check, the
  // extension will fail to load when chrome is auto-updated.  To avoid
  // this, new parse checks are enabled only when |strictness| is
  // OPTION_STRICT.  OPTION_STRICT should be used when loading in developer
  // mode, or when an extension's patterns are controlled by chrome (such
  // as component extensions).
  ParseResult Parse(const std::string& pattern_str,
                    ParseOption strictness);

  // Sets the scheme for pattern matches. This can be a single '*' if the
  // pattern matches all valid schemes (as defined by the valid_schemes_
  // property). Returns false on failure (if the scheme is not valid).
  bool SetScheme(const std::string& scheme);
  // Note: You should use MatchesScheme() instead of this getter unless you
  // absolutely need the exact scheme. This is exposed for testing.
  const std::string& scheme() const { return scheme_; }

  // Returns true if the specified scheme can be used in this URL pattern, and
  // false otherwise. Uses valid_schemes_ to determine validity.
  bool IsValidScheme(const std::string& scheme) const;

  // Returns true if this instance matches the specified URL.
  bool MatchesUrl(const GURL& url) const;

  // Returns true if |test| matches our scheme.
  bool MatchesScheme(const std::string& test) const;

  // Returns true if |test| matches our host.
  bool MatchesHost(const std::string& test) const;
  bool MatchesHost(const GURL& test) const;

  // Returns true if |test| matches our path.
  bool MatchesPath(const std::string& test) const;

  // Returns a string representing this instance.
  std::string GetAsString() const;

  // Determine whether there is a URL that would match this instance and another
  // instance. This method is symmetrical: Calling other.OverlapsWith(this)
  // would result in the same answer.
  bool OverlapsWith(const URLPattern& other) const;

  // Convert this URLPattern into an equivalent set of URLPatterns that don't
  // use a wildcard in the scheme component. If this URLPattern doesn't use a
  // wildcard scheme, then the returned set will contain one element that is
  // equivalent to this instance.
  std::vector<URLPattern> ConvertToExplicitSchemes() const;

  static bool EffectiveHostCompare(const URLPattern& a, const URLPattern& b) {
    if (a.match_all_urls_ && b.match_all_urls_)
      return false;
    return a.host_.compare(b.host_) < 0;
  };

  // Used for origin comparisons in a std::set.
  class EffectiveHostCompareFunctor {
   public:
    bool operator()(const URLPattern& a, const URLPattern& b) const {
      return EffectiveHostCompare(a, b);
    };
  };

  // Get an error string for a ParseResult.
  static const char* GetParseResultString(URLPattern::ParseResult parse_result);

 private:
#if !(defined(_MSC_VER) && _MSC_VER >= 1600)
  friend class std::vector<URLPattern>;

  // Note: don't use this directly. This exists so URLPattern can be used
  // with STL containers.
  URLPattern();
#endif

  // A bitmask containing the schemes which are considered valid for this
  // pattern. Parse() uses this to decide whether a pattern contains a valid
  // scheme. MatchesScheme uses this to decide whether a wildcard scheme_
  // matches a given test scheme.
  int valid_schemes_;

  // True if this is a special-case "<all_urls>" pattern.
  bool match_all_urls_;

  // The scheme for the pattern.
  std::string scheme_;

  // The host without any leading "*" components.
  std::string host_;

  // Whether we should match subdomains of the host. This is true if the first
  // component of the pattern's host was "*".
  bool match_subdomains_;

  // The path to match. This is everything after the host of the URL, or
  // everything after the scheme in the case of file:// URLs.
  std::string path_;

  // The path with "?" and "\" characters escaped for use with the
  // MatchPattern() function.
  std::string path_escaped_;
};

typedef std::vector<URLPattern> URLPatternList;

#endif  // CHROME_COMMON_EXTENSIONS_URL_PATTERN_H_
