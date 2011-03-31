// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/toolbar_importer_utils.h"

#include <string>
#include <vector>

#include "base/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
const char kGoogleDomainUrl[] = "http://.google.com/";
const char kGoogleDomainSecureCookieId[] = "SID=";
const char kSplitStringToken = ';';
}

namespace toolbar_importer_utils {

bool IsGoogleGAIACookieInstalled() {
  net::CookieStore* store =
      Profile::GetDefaultRequestContext()->DONTUSEME_GetCookieStore();
  GURL url(kGoogleDomainUrl);
  net::CookieOptions options;
  options.set_include_httponly();  // The SID cookie might be httponly.
  std::string cookies = store->GetCookiesWithOptions(url, options);
  std::vector<std::string> cookie_list;
  base::SplitString(cookies, kSplitStringToken, &cookie_list);
  for (std::vector<std::string>::iterator current = cookie_list.begin();
       current != cookie_list.end();
       ++current) {
    size_t position = (*current).find(kGoogleDomainSecureCookieId);
    if (0 == position)
      return true;
  }
  return false;
}

}  // namespace toolbar_importer_utils
