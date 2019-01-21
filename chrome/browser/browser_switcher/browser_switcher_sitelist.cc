// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

#include "third_party/re2/src/re2/re2.h"

namespace browser_switcher {

namespace {

// This type is cheap and lives on the stack, which can be faster compared to
// calling |GURL::host()| multiple times.
struct NoCopyUrl {
  base::StringPiece host;
  base::StringPiece spec;
};

// Returns true if |input| contains |token|, ignoring case for ASCII
// characters.
bool StringContainsInsensitiveASCII(base::StringPiece input,
                                    base::StringPiece token) {
  const char* found =
      std::search(input.begin(), input.end(), token.begin(), token.end(),
                  [](char a, char b) {
                    return base::ToLowerASCII(a) == base::ToLowerASCII(b);
                  });
  return found != input.end();
}

// Checks if the omitted prefix for a non-fully specific prefix is one of the
// expected parts that are allowed to be omitted (e.g. "https://").
bool IsValidPrefix(base::StringPiece prefix) {
  static re2::LazyRE2 re = {"(https?|file):(//)?"};
  re2::StringPiece converted_prefix(prefix.data(), prefix.size());
  return (prefix.empty() || re2::RE2::FullMatch(converted_prefix, *re));
}

bool IsInverted(base::StringPiece pattern) {
  return (!pattern.empty() && pattern[0] == '!');
}

bool UrlMatchesPattern(const NoCopyUrl& url, base::StringPiece pattern) {
  if (pattern == "*") {
    // Wildcard, always match.
    return true;
  }
  if (pattern.find('/') != base::StringPiece::npos) {
    // Check prefix using the normalized URL, case sensitive.
    size_t pos = url.spec.find(pattern);
    if (pos == base::StringPiece::npos)
      return false;
    return IsValidPrefix(base::StringPiece(url.spec.data(), pos));
  }
  // Compare hosts, case-insensitive.
  return StringContainsInsensitiveASCII(url.host, pattern);
}

// Checks whether |patterns| contains a pattern that matches |url|, and returns
// the longest matching pattern. If there are no matches, an empty pattern is
// returned.
//
// If |contains_inverted_matches| is true, treat patterns that start with "!" as
// inverted matches.
base::StringPiece MatchUrlToList(const NoCopyUrl& url,
                                 const std::vector<std::string>& patterns,
                                 bool contains_inverted_matches) {
  base::StringPiece reason;
  for (const std::string& pattern : patterns) {
    if (pattern.size() <= reason.size())
      continue;
    bool inverted = IsInverted(pattern);
    if (inverted && !contains_inverted_matches)
      continue;
    if (UrlMatchesPattern(url, (inverted ? pattern.substr(1) : pattern))) {
      reason = pattern;
    }
  }
  return reason;
}

bool StringSizeCompare(const base::StringPiece& a, const base::StringPiece& b) {
  return a.size() < b.size();
}

}  // namespace

BrowserSwitcherSitelist::~BrowserSwitcherSitelist() = default;

BrowserSwitcherSitelistImpl::BrowserSwitcherSitelistImpl(
    const BrowserSwitcherPrefs* prefs)
    : prefs_(prefs) {}

BrowserSwitcherSitelistImpl::~BrowserSwitcherSitelistImpl() = default;

bool BrowserSwitcherSitelistImpl::ShouldSwitch(const GURL& url) const {
  // Don't record metrics for LBS non-users.
  if (!IsActive())
    return false;

  bool should_switch = ShouldSwitchImpl(url);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.Decision", should_switch);
  return should_switch;
}

bool BrowserSwitcherSitelistImpl::ShouldSwitchImpl(const GURL& url) const {
  SCOPED_UMA_HISTOGRAM_TIMER("BrowserSwitcher.DecisionTime");

  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile()) {
    return false;
  }

  std::string url_host = url.host();
  NoCopyUrl no_copy_url = {url_host, url.spec()};

  base::StringPiece reason_to_go = std::max(
      {
          MatchUrlToList(no_copy_url, prefs_->GetRules().sitelist, true),
          MatchUrlToList(no_copy_url, ieem_sitelist_.sitelist, true),
          MatchUrlToList(no_copy_url, external_sitelist_.sitelist, true),
      },
      StringSizeCompare);

  // If sitelists don't match, no need to check the greylists.
  if (reason_to_go.empty() || IsInverted(reason_to_go)) {
    return false;
  }

  base::StringPiece reason_to_stay = std::max(
      {
          MatchUrlToList(no_copy_url, prefs_->GetRules().greylist, false),
          MatchUrlToList(no_copy_url, ieem_sitelist_.greylist, false),
          MatchUrlToList(no_copy_url, external_sitelist_.greylist, false),
      },
      StringSizeCompare);

  if (reason_to_go == "*" && !reason_to_stay.empty())
    return false;

  return reason_to_go.size() >= reason_to_stay.size();
}

void BrowserSwitcherSitelistImpl::SetIeemSitelist(ParsedXml&& parsed_xml) {
  DCHECK(!parsed_xml.error);

  UMA_HISTOGRAM_COUNTS_100000(
      "BrowserSwitcher.IeemSitelistSize",
      parsed_xml.sitelist.size() + parsed_xml.greylist.size());

  ieem_sitelist_.sitelist = std::move(parsed_xml.sitelist);
  ieem_sitelist_.greylist = std::move(parsed_xml.greylist);
}

void BrowserSwitcherSitelistImpl::SetExternalSitelist(ParsedXml&& parsed_xml) {
  DCHECK(!parsed_xml.error);

  UMA_HISTOGRAM_COUNTS_100000("BrowserSwitcher.ExternalSitelistSize",
                              parsed_xml.sitelist.size());

  external_sitelist_.sitelist = std::move(parsed_xml.sitelist);
  external_sitelist_.greylist = std::move(parsed_xml.greylist);
}

bool BrowserSwitcherSitelistImpl::IsActive() const {
  if (!prefs_->IsEnabled())
    return false;

  const RuleSet* rulesets[] = {&prefs_->GetRules(), &ieem_sitelist_,
                               &external_sitelist_};
  for (const RuleSet* rules : rulesets) {
    if (!rules->sitelist.empty() || !rules->greylist.empty())
      return true;
  }
  return false;
}

}  // namespace browser_switcher
