// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_util.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/google/core/common/google_util.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace policy {
namespace url_util {

namespace {

// Host/regex pattern for Google AMP Cache URLs.
// See https://developers.google.com/amp/cache/overview#amp-cache-url-format
// for a definition of the format of AMP Cache URLs.
const char kGoogleAmpCacheHost[] = "cdn.ampproject.org";
const char kGoogleAmpCachePathPattern[] = "/[a-z]/(s/)?(.*)";

// Regex pattern for the path of Google AMP Viewer URLs.
const char kGoogleAmpViewerPathPattern[] = "/amp/(s/)?(.*)";

// Host, path prefix, and query regex pattern for Google web cache URLs.
const char kGoogleWebCacheHost[] = "webcache.googleusercontent.com";
const char kGoogleWebCachePathPrefix[] = "/search";
const char kGoogleWebCacheQueryPattern[] =
    "cache:(.{12}:)?(https?://)?([^ :]*)( [^:]*)?";

const char kGoogleTranslateSubdomain[] = "translate.";
const char kAlternateGoogleTranslateHost[] = "translate.googleusercontent.com";

// Returns a full URL using either "http" or "https" as the scheme.
GURL BuildURL(bool is_https, const std::string& host_and_path) {
  std::string scheme = is_https ? url::kHttpsScheme : url::kHttpScheme;
  return GURL(scheme + "://" + host_and_path);
}

// Helper class for testing the URL against precompiled regexes. This is a
// singleton so the cached regexes are only created once.
class EmbeddedURLExtractor {
 public:
  static EmbeddedURLExtractor* GetInstance() {
    static base::NoDestructor<EmbeddedURLExtractor> instance;
    return instance.get();
  }

  // Implements url_filter::GetEmbeddedURL().
  GURL GetEmbeddedURL(const GURL& url) {
    // Check for "*.cdn.ampproject.org" URLs.
    if (url.DomainIs(kGoogleAmpCacheHost)) {
      std::string s;
      std::string embedded;
      if (re2::RE2::FullMatch(url.path(), google_amp_cache_path_regex_, &s,
                              &embedded)) {
        if (url.has_query())
          embedded += "?" + url.query();
        return BuildURL(!s.empty(), embedded);
      }
    }

    // Check for "www.google.TLD/amp/" URLs.
    if (google_util::IsGoogleDomainUrl(
            url, google_util::DISALLOW_SUBDOMAIN,
            google_util::DISALLOW_NON_STANDARD_PORTS)) {
      std::string s;
      std::string embedded;
      if (re2::RE2::FullMatch(url.path(), google_amp_viewer_path_regex_, &s,
                              &embedded)) {
        // The embedded URL may be percent-encoded. Undo that.
        embedded = net::UnescapeBinaryURLComponent(embedded);
        return BuildURL(!s.empty(), embedded);
      }
    }

    // Check for Google web cache URLs
    // ("webcache.googleusercontent.com/search?q=cache:...").
    std::string query;
    if (url.host_piece() == kGoogleWebCacheHost &&
        url.path_piece().starts_with(kGoogleWebCachePathPrefix) &&
        net::GetValueForKeyInQuery(url, "q", &query)) {
      std::string fingerprint;
      std::string scheme;
      std::string embedded;
      if (re2::RE2::FullMatch(query, google_web_cache_query_regex_,
                              &fingerprint, &scheme, &embedded)) {
        return BuildURL(scheme == "https://", embedded);
      }
    }

    // Check for Google translate URLs ("translate.google.TLD/...?...&u=URL" or
    // "translate.googleusercontent.com/...?...&u=URL").
    bool is_translate = false;
    if (base::StartsWith(url.host_piece(), kGoogleTranslateSubdomain,
                         base::CompareCase::SENSITIVE)) {
      // Remove the "translate." prefix.
      GURL::Replacements replace;
      replace.SetHostStr(
          url.host_piece().substr(strlen(kGoogleTranslateSubdomain)));
      GURL trimmed = url.ReplaceComponents(replace);
      // Check that the remainder is a Google URL. Note: IsGoogleDomainUrl
      // checks for [www.]google.TLD, but we don't want the "www.", so
      // explicitly exclude that.
      // TODO(treib,pam): Instead of excluding "www." manually, teach
      // IsGoogleDomainUrl a mode that doesn't allow it.
      is_translate = google_util::IsGoogleDomainUrl(
                         trimmed, google_util::DISALLOW_SUBDOMAIN,
                         google_util::DISALLOW_NON_STANDARD_PORTS) &&
                     !base::StartsWith(trimmed.host_piece(), "www.",
                                       base::CompareCase::SENSITIVE);
    }
    bool is_alternate_translate =
        url.host_piece() == kAlternateGoogleTranslateHost;
    if (is_translate || is_alternate_translate) {
      std::string embedded;
      if (net::GetValueForKeyInQuery(url, "u", &embedded)) {
        // The embedded URL may or may not include a scheme. Fix it if
        // necessary.
        return url_formatter::FixupURL(embedded, /*desired_tld=*/std::string());
      }
    }

    return GURL();
  }

 private:
  friend class base::NoDestructor<EmbeddedURLExtractor>;

  EmbeddedURLExtractor()
      : google_amp_cache_path_regex_(kGoogleAmpCachePathPattern),
        google_amp_viewer_path_regex_(kGoogleAmpViewerPathPattern),
        google_web_cache_query_regex_(kGoogleWebCacheQueryPattern) {
    DCHECK(google_amp_cache_path_regex_.ok());
    DCHECK(google_amp_viewer_path_regex_.ok());
    DCHECK(google_web_cache_query_regex_.ok());
  }

  ~EmbeddedURLExtractor() = default;

  const re2::RE2 google_amp_cache_path_regex_;
  const re2::RE2 google_amp_viewer_path_regex_;
  const re2::RE2 google_web_cache_query_regex_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedURLExtractor);
};

}  // namespace

GURL Normalize(const GURL& url) {
  GURL normalized_url = url;
  GURL::Replacements replacements;
  // Strip username, password, query, and ref.
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

GURL GetEmbeddedURL(const GURL& url) {
  return EmbeddedURLExtractor::GetInstance()->GetEmbeddedURL(url);
}

}  // namespace url_util
}  // namespace policy
