// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_util.h"

#include "base/json/json_writer.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/web/navigation/placeholder_navigation_util.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_client.h"
#include "net/base/url_util.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace wk_navigation_util {

const char kRestoreSessionSessionQueryKey[] = "session";
const char kRestoreSessionTargetUrlQueryKey[] = "targetUrl";

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

  // The URLs and titles of the restored entries are stored in two separate
  // lists instead of a single list of objects to reduce the size of the JSON
  // string to be included in the query parameter.
  base::Value restored_urls(base::Value::Type::LIST);
  base::Value restored_titles(base::Value::Type::LIST);
  restored_urls.GetList().reserve(items.size());
  restored_titles.GetList().reserve(items.size());
  for (size_t index = 0; index < items.size(); index++) {
    GURL original_url = items[index]->GetURL();
    GURL restored_url = original_url;
    if (web::GetWebClient()->IsAppSpecificURL(original_url)) {
      restored_url =
          placeholder_navigation_util::CreatePlaceholderUrlForUrl(original_url);
    }
    restored_urls.GetList().push_back(base::Value(restored_url.spec()));
    restored_titles.GetList().push_back(base::Value(items[index]->GetTitle()));
  }
  base::Value session(base::Value::Type::DICTIONARY);
  int offset = last_committed_item_index + 1 - items.size();
  session.SetKey("offset", base::Value(offset));
  session.SetKey("urls", std::move(restored_urls));
  session.SetKey("titles", std::move(restored_titles));

  std::string session_json;
  base::JSONWriter::Write(session, &session_json);
  return net::AppendQueryParameter(
      GetRestoreSessionBaseUrl(), kRestoreSessionSessionQueryKey, session_json);
}

bool IsRestoreSessionUrl(const GURL& url) {
  return url.SchemeIsFile() && url.path() == GetRestoreSessionBaseUrl().path();
}

GURL CreateRedirectUrl(const GURL& target_url) {
  return net::AppendQueryParameter(GetRestoreSessionBaseUrl(),
                                   kRestoreSessionTargetUrlQueryKey,
                                   target_url.spec());
}

bool ExtractTargetURL(const GURL& restore_session_url, GURL* target_url) {
  DCHECK(IsRestoreSessionUrl(restore_session_url))
      << restore_session_url.possibly_invalid_spec()
      << " is not a restore session URL";
  std::string target_url_spec;
  bool success = net::GetValueForKeyInQuery(
      restore_session_url, kRestoreSessionTargetUrlQueryKey, &target_url_spec);
  if (success)
    *target_url = GURL(target_url_spec);

  return success;
}

}  // namespace wk_navigation_util
}  // namespace web
