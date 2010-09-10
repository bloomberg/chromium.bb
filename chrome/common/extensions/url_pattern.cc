// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/url_pattern.h"

#include "base/string_piece.h"
#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

// TODO(aa): Consider adding chrome-extension? What about more obscure ones
// like data: and javascript: ?
// Note: keep this array in sync with kValidSchemeMasks.
static const char* kValidSchemes[] = {
  chrome::kHttpScheme,
  chrome::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFtpScheme,
  chrome::kChromeUIScheme,
};

static const int kValidSchemeMasks[] = {
  URLPattern::SCHEME_HTTP,
  URLPattern::SCHEME_HTTPS,
  URLPattern::SCHEME_FILE,
  URLPattern::SCHEME_FTP,
  URLPattern::SCHEME_CHROMEUI,
};

COMPILE_ASSERT(arraysize(kValidSchemes) == arraysize(kValidSchemeMasks),
               must_keep_these_arrays_in_sync);

static const char kPathSeparator[] = "/";

static const char kAllUrlsPattern[] = "<all_urls>";

URLPattern::URLPattern()
    : valid_schemes_(0), match_all_urls_(false), match_subdomains_(false) {}

URLPattern::URLPattern(int valid_schemes)
    : valid_schemes_(valid_schemes), match_all_urls_(false),
      match_subdomains_(false) {}

URLPattern::URLPattern(int valid_schemes, const std::string& pattern)
    : valid_schemes_(valid_schemes), match_all_urls_(false),
      match_subdomains_(false) {
  if (!Parse(pattern))
    NOTREACHED() << "URLPattern is invalid: " << pattern;
}

URLPattern::~URLPattern() {
}

bool URLPattern::Parse(const std::string& pattern) {
  // Special case pattern to match every valid URL.
  if (pattern == kAllUrlsPattern) {
    match_all_urls_ = true;
    match_subdomains_ = true;
    scheme_ = "*";
    host_.clear();
    path_ = "/*";
    return true;
  }

  size_t scheme_end_pos = pattern.find(chrome::kStandardSchemeSeparator);
  if (scheme_end_pos == std::string::npos)
    return false;

  if (!SetScheme(pattern.substr(0, scheme_end_pos)))
    return false;

  size_t host_start_pos = scheme_end_pos +
      strlen(chrome::kStandardSchemeSeparator);
  if (host_start_pos >= pattern.length())
    return false;

  // Parse out the host and path.
  size_t path_start_pos = 0;

  // File URLs are special because they have no host. There are other schemes
  // with the same structure, but we don't support them (yet).
  if (scheme_ == chrome::kFileScheme) {
    path_start_pos = host_start_pos;
  } else {
    size_t host_end_pos = pattern.find(kPathSeparator, host_start_pos);
    if (host_end_pos == std::string::npos)
      return false;

    host_ = pattern.substr(host_start_pos, host_end_pos - host_start_pos);

    // The first component can optionally be '*' to match all subdomains.
    std::vector<std::string> host_components;
    SplitString(host_, '.', &host_components);
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
      return false;

    path_start_pos = host_end_pos;
  }

  path_ = pattern.substr(path_start_pos);

  return true;
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
  for (size_t i = 0; i < arraysize(kValidSchemes); ++i) {
    if (scheme == kValidSchemes[i] && (valid_schemes_ & kValidSchemeMasks[i]))
      return true;
  }

  return false;
}

bool URLPattern::MatchesUrl(const GURL &test) const {
  if (!MatchesScheme(test.scheme()))
    return false;

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
  if (path_escaped_.empty()) {
    path_escaped_ = path_;
    ReplaceSubstringsAfterOffset(&path_escaped_, 0, "\\", "\\\\");
    ReplaceSubstringsAfterOffset(&path_escaped_, 0, "?", "\\?");
  }

  if (!MatchPattern(test, path_escaped_))
    return false;

  return true;
}

std::string URLPattern::GetAsString() const {
  if (match_all_urls_)
    return kAllUrlsPattern;

  std::string spec = scheme_ + chrome::kStandardSchemeSeparator;

  if (scheme_ != chrome::kFileScheme) {
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
