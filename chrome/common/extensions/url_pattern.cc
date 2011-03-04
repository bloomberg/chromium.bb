// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/url_pattern.h"

#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"

const char URLPattern::kAllUrlsPattern[] = "<all_urls>";

namespace {

// TODO(aa): Consider adding chrome-extension? What about more obscure ones
// like data: and javascript: ?
// Note: keep this array in sync with kValidSchemeMasks.
const char* kValidSchemes[] = {
  chrome::kHttpScheme,
  chrome::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFtpScheme,
  chrome::kChromeUIScheme,
};

const int kValidSchemeMasks[] = {
  URLPattern::SCHEME_HTTP,
  URLPattern::SCHEME_HTTPS,
  URLPattern::SCHEME_FILE,
  URLPattern::SCHEME_FTP,
  URLPattern::SCHEME_CHROMEUI,
};

COMPILE_ASSERT(arraysize(kValidSchemes) == arraysize(kValidSchemeMasks),
               must_keep_these_arrays_in_sync);

const char* kParseSuccess = "Success.";
const char* kParseErrorMissingSchemeSeparator = "Missing scheme separator.";
const char* kParseErrorInvalidScheme = "Invalid scheme.";
const char* kParseErrorWrongSchemeType = "Wrong scheme type.";
const char* kParseErrorEmptyHost = "Host can not be empty.";
const char* kParseErrorInvalidHostWildcard = "Invalid host wildcard.";
const char* kParseErrorEmptyPath = "Empty path.";
const char* kParseErrorHasColon =
    "Ports are not supported in URL patterns. ':' may not be used in a host.";

// Message explaining each URLPattern::ParseResult.
const char* kParseResultMessages[] = {
  kParseSuccess,
  kParseErrorMissingSchemeSeparator,
  kParseErrorInvalidScheme,
  kParseErrorWrongSchemeType,
  kParseErrorEmptyHost,
  kParseErrorInvalidHostWildcard,
  kParseErrorEmptyPath,
  kParseErrorHasColon
};

COMPILE_ASSERT(URLPattern::NUM_PARSE_RESULTS == arraysize(kParseResultMessages),
               must_add_message_for_each_parse_result);

const char kPathSeparator[] = "/";

bool IsStandardScheme(const std::string& scheme) {
  // "*" gets the same treatment as a standard scheme.
  if (scheme == "*")
    return true;

  return url_util::IsStandard(scheme.c_str(),
      url_parse::Component(0, static_cast<int>(scheme.length())));
}

}  // namespace

URLPattern::URLPattern()
    : valid_schemes_(SCHEME_NONE),
      match_all_urls_(false),
      match_subdomains_(false) {}

URLPattern::URLPattern(int valid_schemes)
    : valid_schemes_(valid_schemes), match_all_urls_(false),
      match_subdomains_(false) {}

URLPattern::URLPattern(int valid_schemes, const std::string& pattern)
    : valid_schemes_(valid_schemes), match_all_urls_(false),
      match_subdomains_(false) {

  // Strict error checking is used, because this constructor is only
  // appropriate when we know |pattern| is valid.
  if (PARSE_SUCCESS != Parse(pattern, PARSE_STRICT))
    NOTREACHED() << "URLPattern is invalid: " << pattern;
}

URLPattern::~URLPattern() {
}

URLPattern::ParseResult URLPattern::Parse(const std::string& pattern,
                                          ParseOption strictness) {
  CHECK(strictness == PARSE_LENIENT ||
        strictness == PARSE_STRICT);

  // Special case pattern to match every valid URL.
  if (pattern == kAllUrlsPattern) {
    match_all_urls_ = true;
    match_subdomains_ = true;
    scheme_ = "*";
    host_.clear();
    SetPath("/*");
    return PARSE_SUCCESS;
  }

  // Parse out the scheme.
  size_t scheme_end_pos = pattern.find(chrome::kStandardSchemeSeparator);
  bool has_standard_scheme_separator = true;

  // Some urls also use ':' alone as the scheme separator.
  if (scheme_end_pos == std::string::npos) {
    scheme_end_pos = pattern.find(':');
    has_standard_scheme_separator = false;
  }

  if (scheme_end_pos == std::string::npos)
    return PARSE_ERROR_MISSING_SCHEME_SEPARATOR;

  if (!SetScheme(pattern.substr(0, scheme_end_pos)))
    return PARSE_ERROR_INVALID_SCHEME;

  bool standard_scheme = IsStandardScheme(scheme_);
  if (standard_scheme != has_standard_scheme_separator)
    return PARSE_ERROR_WRONG_SCHEME_SEPARATOR;

  // Advance past the scheme separator.
  scheme_end_pos +=
      (standard_scheme ? strlen(chrome::kStandardSchemeSeparator) : 1);
  if (scheme_end_pos >= pattern.size())
    return PARSE_ERROR_EMPTY_HOST;

  // Parse out the host and path.
  size_t host_start_pos = scheme_end_pos;
  size_t path_start_pos = 0;

  // File URLs are special because they have no host.
  if (scheme_ == chrome::kFileScheme || !standard_scheme) {
    path_start_pos = host_start_pos;
  } else {
    size_t host_end_pos = pattern.find(kPathSeparator, host_start_pos);

    // Host is required.
    if (host_start_pos == host_end_pos)
      return PARSE_ERROR_EMPTY_HOST;

    if (host_end_pos == std::string::npos)
      return PARSE_ERROR_EMPTY_PATH;

    host_ = pattern.substr(host_start_pos, host_end_pos - host_start_pos);

    // The first component can optionally be '*' to match all subdomains.
    std::vector<std::string> host_components;
    base::SplitString(host_, '.', &host_components);
    if (host_components[0] == "*") {
      match_subdomains_ = true;
      host_components.erase(host_components.begin(),
                            host_components.begin() + 1);
    }
    host_ = JoinString(host_components, '.');

    // No other '*' can occur in the host, though. This isn't necessary, but is
    // done as a convenience to developers who might otherwise be confused and
    // think '*' works as a glob in the host.
    if (host_.find('*') != std::string::npos)
      return PARSE_ERROR_INVALID_HOST_WILDCARD;

    path_start_pos = host_end_pos;
  }

  SetPath(pattern.substr(path_start_pos));

  if (strictness == PARSE_STRICT && host_.find(':') != std::string::npos)
    return PARSE_ERROR_HAS_COLON;

  return PARSE_SUCCESS;
}

bool URLPattern::SetScheme(const std::string& scheme) {
  scheme_ = scheme;
  if (scheme_ == "*") {
    valid_schemes_ &= (SCHEME_HTTP | SCHEME_HTTPS);
  } else if (!IsValidScheme(scheme_)) {
    return false;
  }
  return true;
}

bool URLPattern::IsValidScheme(const std::string& scheme) const {
  if (valid_schemes_ == SCHEME_ALL)
    return true;

  for (size_t i = 0; i < arraysize(kValidSchemes); ++i) {
    if (scheme == kValidSchemes[i] && (valid_schemes_ & kValidSchemeMasks[i]))
      return true;
  }

  return false;
}

void URLPattern::SetPath(const std::string& path) {
  path_ = path;
  path_escaped_ = path_;
  ReplaceSubstringsAfterOffset(&path_escaped_, 0, "\\", "\\\\");
  ReplaceSubstringsAfterOffset(&path_escaped_, 0, "?", "\\?");
}

bool URLPattern::MatchesUrl(const GURL &test) const {
  if (!MatchesScheme(test.scheme()))
    return false;

  if (match_all_urls_)
    return true;

  if (!MatchesHost(test))
    return false;

  if (!MatchesPath(test.PathForRequest()))
    return false;

  return true;
}

bool URLPattern::MatchesScheme(const std::string& test) const {
  if (scheme_ == "*")
    return IsValidScheme(test);

  return test == scheme_;
}

bool URLPattern::MatchesHost(const std::string& host) const {
  std::string test(chrome::kHttpScheme);
  test += chrome::kStandardSchemeSeparator;
  test += host;
  test += "/";
  return MatchesHost(GURL(test));
}

bool URLPattern::MatchesHost(const GURL& test) const {
  // If the hosts are exactly equal, we have a match.
  if (test.host() == host_)
    return true;

  // If we're matching subdomains, and we have no host in the match pattern,
  // that means that we're matching all hosts, which means we have a match no
  // matter what the test host is.
  if (match_subdomains_ && host_.empty())
    return true;

  // Otherwise, we can only match if our match pattern matches subdomains.
  if (!match_subdomains_)
    return false;

  // We don't do subdomain matching against IP addresses, so we can give up now
  // if the test host is an IP address.
  if (test.HostIsIPAddress())
    return false;

  // Check if the test host is a subdomain of our host.
  if (test.host().length() <= (host_.length() + 1))
    return false;

  if (test.host().compare(test.host().length() - host_.length(),
                          host_.length(), host_) != 0)
    return false;

  return test.host()[test.host().length() - host_.length() - 1] == '.';
}

bool URLPattern::MatchesPath(const std::string& test) const {
  if (!MatchPattern(test, path_escaped_))
    return false;

  return true;
}

std::string URLPattern::GetAsString() const {
  if (match_all_urls_)
    return kAllUrlsPattern;

  bool standard_scheme = IsStandardScheme(scheme_);

  std::string spec = scheme_ +
      (standard_scheme ? chrome::kStandardSchemeSeparator : ":");

  if (scheme_ != chrome::kFileScheme && standard_scheme) {
    if (match_subdomains_) {
      spec += "*";
      if (!host_.empty())
        spec += ".";
    }

    if (!host_.empty())
      spec += host_;
  }

  if (!path_.empty())
    spec += path_;

  return spec;
}

bool URLPattern::OverlapsWith(const URLPattern& other) const {
  if (!MatchesScheme(other.scheme_) && !other.MatchesScheme(scheme_))
    return false;

  if (!MatchesHost(other.host()) && !other.MatchesHost(host_))
    return false;

  // We currently only use OverlapsWith() for the patterns inside
  // ExtensionExtent. In those cases, we know that the path will have only a
  // single wildcard at the end. This makes figuring out overlap much easier. It
  // seems like there is probably a computer-sciency way to solve the general
  // case, but we don't need that yet.
  DCHECK(path_.find('*') == path_.size() - 1);
  DCHECK(other.path().find('*') == other.path().size() - 1);

  if (!MatchesPath(other.path().substr(0, other.path().size() - 1)) &&
      !other.MatchesPath(path_.substr(0, path_.size() - 1)))
    return false;

  return true;
}

std::vector<URLPattern> URLPattern::ConvertToExplicitSchemes() const {
  std::vector<URLPattern> result;

  if (scheme_ != "*" && !match_all_urls_) {
    result.push_back(*this);
    return result;
  }

  for (size_t i = 0; i < arraysize(kValidSchemes); ++i) {
    if (MatchesScheme(kValidSchemes[i])) {
      URLPattern temp = *this;
      temp.SetScheme(kValidSchemes[i]);
      temp.set_match_all_urls(false);
      result.push_back(temp);
    }
  }

  return result;
}

// static
const char* URLPattern::GetParseResultString(
    URLPattern::ParseResult parse_result) {
  return kParseResultMessages[parse_result];
}
