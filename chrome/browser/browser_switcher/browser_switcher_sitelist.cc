// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

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

// URL is passed as a (url_spec, url_host) to avoid heap-allocating a string for
// the host every time this is called.
bool UrlMatches(base::StringPiece url_spec,
                base::StringPiece url_host,
                base::StringPiece pattern) {
  if (pattern == "*") {
    // Wildcard, always match.
    return true;
  }
  if (pattern.find('/') != base::StringPiece::npos) {
    // Check prefix using the normalized URL, case sensitive.
    return base::StartsWith(url_spec, GURL(pattern).spec(),
                            base::CompareCase::SENSITIVE);
  }
  // Compare hosts, case-insensitive.
  return StringContainsInsensitiveASCII(url_host, pattern);
}

// Checks whether |patterns| contains a pattern that matches |url|, and puts the
// longest matching pattern in |*reason|.
//
// If |contains_inverted_matches| is true, treat patterns that start with "!" as
// inverted matches (which return false if matched).
bool UrlListMatches(const GURL& url,
                    const base::ListValue* patterns,
                    bool contains_inverted_matches,
                    base::StringPiece* reason) {
  const std::string url_host = url.host();
  const std::string& url_spec = url.spec();
  *reason = "";
  bool matched = false;
  for (const base::Value& pattern_value : *patterns) {
    if (!pattern_value.is_string())
      continue;
    base::StringPiece pattern = pattern_value.GetString();
    if (pattern.size() <= reason->size())
      continue;
    bool inverted =
        base::StartsWith(pattern, "!", base::CompareCase::SENSITIVE);
    if (inverted && !contains_inverted_matches)
      continue;
    if (UrlMatches(url_spec, url_host,
                   (inverted ? pattern.substr(1) : pattern))) {
      matched = !inverted;
      *reason = pattern;
    }
  }
  return matched;
}

}  // namespace

BrowserSwitcherSitelist::BrowserSwitcherSitelist(PrefService* prefs)
    : prefs_(prefs) {
  DCHECK(prefs_);
}

BrowserSwitcherSitelist::~BrowserSwitcherSitelist() {}

bool BrowserSwitcherSitelist::ShouldSwitch(const GURL& url) const {
  // Translated from the LBS extension:
  // https://github.com/LegacyBrowserSupport/legacy-browser-support/blob/8caa623692b94dc0154074ce904de8f60ee8a404/chrome_extension/js/extension_logic.js#L205
  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile()) {
    return false;
  }

  const base::ListValue* url_list = prefs_->GetList(prefs::kUrlList);
  base::StringPiece reason_to_go;
  bool should_go = UrlListMatches(url, url_list, true, &reason_to_go);

  const base::ListValue* url_greylist = prefs_->GetList(prefs::kUrlGreylist);
  base::StringPiece reason_to_stay;
  bool should_stay = UrlListMatches(url, url_greylist, false, &reason_to_stay);

  // Always prefer the more specific entry of the two lists.
  if (should_stay) {
    if (reason_to_go == "*")
      return false;
    if (!reason_to_go.empty() && reason_to_go.size() < reason_to_stay.size())
      return false;
  }
  return should_go;
}

}  // namespace browser_switcher
