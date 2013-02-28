// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/toolbar_importer_utils.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {
const char kGoogleDomainUrl[] = "http://.google.com/";
const char kGoogleDomainSecureCookieId[] = "SID=";
const char kSplitStringToken = ';';
}

namespace toolbar_importer_utils {

void OnGetCookies(const base::Callback<void(bool)>& callback,
                  const std::string& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<std::string> cookie_list;
  base::SplitString(cookies, kSplitStringToken, &cookie_list);
  for (std::vector<std::string>::iterator current = cookie_list.begin();
       current != cookie_list.end();
       ++current) {
    size_t position = (*current).find(kGoogleDomainSecureCookieId);
    if (position == 0) {
      callback.Run(true);
      return;
    }
  }
  callback.Run(false);
}

void OnFetchComplete(const base::Callback<void(bool)>& callback,
                     const std::string& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnGetCookies, callback, cookies));
}

void FetchCookiesOnIOThread(
    const base::Callback<void(bool)>& callback,
    net::URLRequestContextGetter* context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* store = context_getter->
      GetURLRequestContext()->cookie_store();
  GURL url(kGoogleDomainUrl);
  net::CookieOptions options;
  options.set_include_httponly();  // The SID cookie might be httponly.
  store->GetCookiesWithOptionsAsync(
      url, options,
      base::Bind(&toolbar_importer_utils::OnFetchComplete, callback));
}

void IsGoogleGAIACookieInstalled(const base::Callback<void(bool)>& callback,
                                 Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&FetchCookiesOnIOThread,
                   callback, base::Unretained(profile->GetRequestContext())));
  }
}

}  // namespace toolbar_importer_utils
