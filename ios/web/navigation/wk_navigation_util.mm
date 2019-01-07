// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_util.h"

#include "base/json/json_writer.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_client.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace wk_navigation_util {

// Session restoration algorithms uses pushState calls to restore back forward
// navigation list. WKWebView does not allow pushing more than 100 items per
// 30 seconds. Limiting max session size to 75 will allow web pages to use push
// state calls.
const int kMaxSessionSize = 75;

const char kRestoreSessionSessionHashPrefix[] = "session=";
const char kRestoreSessionTargetUrlHashPrefix[] = "targetUrl=";
const char kOriginalUrlKey[] = "for";

namespace {
// Returns begin and end iterators for the given navigation items. The length of
// these iterators range will not exceed kMaxSessionSize. If |items.size()| is
// greater than kMaxSessionSize, then this function will trim navigation items,
// which are the furthest to |last_committed_item_index|.
void GetSafeItemIterators(
    int last_committed_item_index,
    const std::vector<std::unique_ptr<NavigationItem>>& items,
    std::vector<std::unique_ptr<NavigationItem>>::const_iterator* begin,
    std::vector<std::unique_ptr<NavigationItem>>::const_iterator* end) {
  if (items.size() <= kMaxSessionSize) {
    // No need to trim anything.
    *begin = items.begin();
    *end = items.end();
    return;
  }

  if (last_committed_item_index < kMaxSessionSize / 2) {
    // Items which are the furthest to |last_committed_item_index| are located
    // on the right side of the vector. Trim those.
    *begin = items.begin();
    *end = items.begin() + kMaxSessionSize;
    return;
  }

  if (items.size() - last_committed_item_index < kMaxSessionSize / 2) {
    // Items which are the furthest to |last_committed_item_index| are located
    // on the left side of the vector. Trim those.
    *begin = items.end() - kMaxSessionSize;
    *end = items.end();
    return;
  }

  // Trim items from both sides of the vector. Keep the same number of items
  // on the left and right side of |last_committed_item_index|.
  *begin = items.begin() + last_committed_item_index - kMaxSessionSize / 2;
  *end = items.begin() + last_committed_item_index + kMaxSessionSize / 2 + 1;
}
}

bool IsWKInternalUrl(const GURL& url) {
  return IsPlaceholderUrl(url) || IsRestoreSessionUrl(url);
}

bool URLNeedsUserAgentType(const GURL& url) {
  return !web::GetWebClient()->IsAppSpecificURL(url) &&
         !(url.SchemeIs(url::kFileScheme) && !IsRestoreSessionUrl(url)) &&
         !(url.SchemeIs(url::kAboutScheme) && !IsPlaceholderUrl(url));
}

GURL GetRestoreSessionBaseUrl() {
  std::string restore_session_resource_path = base::SysNSStringToUTF8(
      [base::mac::FrameworkBundle() pathForResource:@"restore_session"
                                             ofType:@"html"]);
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kFileScheme);
  replacements.SetPathStr(restore_session_resource_path);
  return GURL(url::kAboutBlankURL).ReplaceComponents(replacements);
}

GURL CreateRestoreSessionUrl(
    int last_committed_item_index,
    const std::vector<std::unique_ptr<NavigationItem>>& items) {
  DCHECK(last_committed_item_index >= 0 &&
         last_committed_item_index < static_cast<int>(items.size()));

  std::vector<std::unique_ptr<NavigationItem>>::const_iterator begin;
  std::vector<std::unique_ptr<NavigationItem>>::const_iterator end;
  GetSafeItemIterators(last_committed_item_index, items, &begin, &end);
  size_t new_size = end - begin;

  // The URLs and titles of the restored entries are stored in two separate
  // lists instead of a single list of objects to reduce the size of the JSON
  // string to be included in the query parameter.
  base::Value restored_urls(base::Value::Type::LIST);
  base::Value restored_titles(base::Value::Type::LIST);
  restored_urls.GetList().reserve(new_size);
  restored_titles.GetList().reserve(new_size);
  for (auto it = begin; it != end; ++it) {
    NavigationItem* item = (*it).get();
    GURL original_url = item->GetURL();
    GURL restored_url = original_url;
    if (web::GetWebClient()->IsAppSpecificURL(original_url)) {
      restored_url = CreatePlaceholderUrlForUrl(original_url);
    }
    restored_urls.GetList().push_back(base::Value(restored_url.spec()));
    restored_titles.GetList().push_back(base::Value(item->GetTitle()));
  }
  base::Value session(base::Value::Type::DICTIONARY);
  int offset = last_committed_item_index + 1 - new_size;
  session.SetKey("offset", base::Value(offset));
  session.SetKey("urls", std::move(restored_urls));
  session.SetKey("titles", std::move(restored_titles));

  std::string session_json;
  base::JSONWriter::Write(session, &session_json);
  std::string ref =
      kRestoreSessionSessionHashPrefix +
      net::EscapeQueryParamValue(session_json, false /* use_plus */);
  GURL::Replacements replacements;
  replacements.SetRefStr(ref);
  return GetRestoreSessionBaseUrl().ReplaceComponents(replacements);
}

bool IsRestoreSessionUrl(const GURL& url) {
  return url.SchemeIsFile() && url.path() == GetRestoreSessionBaseUrl().path();
}

GURL CreateRedirectUrl(const GURL& target_url) {
  GURL::Replacements replacements;
  std::string ref =
      kRestoreSessionTargetUrlHashPrefix +
      net::EscapeQueryParamValue(target_url.spec(), false /* use_plus */);
  replacements.SetRefStr(ref);
  return GetRestoreSessionBaseUrl().ReplaceComponents(replacements);
}

bool ExtractTargetURL(const GURL& restore_session_url, GURL* target_url) {
  DCHECK(IsRestoreSessionUrl(restore_session_url))
      << restore_session_url.possibly_invalid_spec()
      << " is not a restore session URL";
  std::string target_url_spec;
  bool success =
      restore_session_url.ref().find(kRestoreSessionTargetUrlHashPrefix) == 0;
  if (success) {
    std::string encoded_target_url = restore_session_url.ref().substr(
        strlen(kRestoreSessionTargetUrlHashPrefix));
    net::UnescapeRule::Type unescape_rules =
        net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
        net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS;
    *target_url =
        GURL(net::UnescapeURLComponent(encoded_target_url, unescape_rules));
  }

  return success;
}

bool IsPlaceholderUrl(const GURL& url) {
  return url.IsAboutBlank() && base::StartsWith(url.query(), kOriginalUrlKey,
                                                base::CompareCase::SENSITIVE);
}

GURL CreatePlaceholderUrlForUrl(const GURL& original_url) {
  if (!original_url.is_valid())
    return GURL::EmptyGURL();

  GURL placeholder_url = net::AppendQueryParameter(
      GURL(url::kAboutBlankURL), kOriginalUrlKey, original_url.spec());
  DCHECK(placeholder_url.is_valid());
  return placeholder_url;
}

GURL ExtractUrlFromPlaceholderUrl(const GURL& url) {
  std::string value;
  if (IsPlaceholderUrl(url) &&
      net::GetValueForKeyInQuery(url, kOriginalUrlKey, &value)) {
    GURL decoded_url(value);
    if (decoded_url.is_valid())
      return decoded_url;
  }
  return GURL::EmptyGURL();
}

}  // namespace wk_navigation_util
}  // namespace web
