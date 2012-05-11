// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements common functionality for the Chrome Extensions Cookies API.

#include "chrome/browser/extensions/extension_cookies_helpers.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_cookies_api_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/cookie_util.h"

namespace keys = extension_cookies_api_constants;

namespace extension_cookies_helpers {

static const char kOriginalProfileStoreId[] = "0";
static const char kOffTheRecordProfileStoreId[] = "1";

Profile* ChooseProfileFromStoreId(const std::string& store_id,
                                  Profile* profile,
                                  bool include_incognito) {
  DCHECK(profile);
  bool allow_original = !profile->IsOffTheRecord();
  bool allow_incognito = profile->IsOffTheRecord() ||
      (include_incognito && profile->HasOffTheRecordProfile());
  if (store_id == kOriginalProfileStoreId && allow_original)
    return profile->GetOriginalProfile();
  if (store_id == kOffTheRecordProfileStoreId && allow_incognito)
    return profile->GetOffTheRecordProfile();
  return NULL;
}

const char* GetStoreIdFromProfile(Profile* profile) {
  DCHECK(profile);
  return profile->IsOffTheRecord() ?
      kOffTheRecordProfileStoreId : kOriginalProfileStoreId;
}

DictionaryValue* CreateCookieValue(
    const net::CookieMonster::CanonicalCookie& cookie,
    const std::string& store_id) {
  DictionaryValue* result = new DictionaryValue();

  // A cookie is a raw byte sequence. By explicitly parsing it as UTF8, we
  // apply error correction, so the string can be safely passed to the
  // renderer.
  result->SetString(keys::kNameKey, UTF8ToUTF16(cookie.Name()));
  result->SetString(keys::kValueKey, UTF8ToUTF16(cookie.Value()));
  result->SetString(keys::kDomainKey, cookie.Domain());
  result->SetBoolean(keys::kHostOnlyKey,
                     net::cookie_util::DomainIsHostOnly(cookie.Domain()));

  // A non-UTF8 path is invalid, so we just replace it with an empty string.
  result->SetString(keys::kPathKey,
                    IsStringUTF8(cookie.Path()) ? cookie.Path() : "");
  result->SetBoolean(keys::kSecureKey, cookie.IsSecure());
  result->SetBoolean(keys::kHttpOnlyKey, cookie.IsHttpOnly());
  result->SetBoolean(keys::kSessionKey, !cookie.DoesExpire());
  if (cookie.DoesExpire()) {
    result->SetDouble(keys::kExpirationDateKey,
                      cookie.ExpiryDate().ToDoubleT());
  }
  result->SetString(keys::kStoreIdKey, store_id);

  return result;
}

DictionaryValue* CreateCookieStoreValue(Profile* profile,
                                        ListValue* tab_ids) {
  DCHECK(profile);
  DCHECK(tab_ids);
  DictionaryValue* result = new DictionaryValue();
  result->SetString(keys::kIdKey, GetStoreIdFromProfile(profile));
  result->Set(keys::kTabIdsKey, tab_ids);
  return result;
}

void GetCookieListFromStore(
    net::CookieStore* cookie_store, const GURL& url,
    const net::CookieMonster::GetCookieListCallback& callback) {
  DCHECK(cookie_store);
  net::CookieMonster* monster = cookie_store->GetCookieMonster();
  if (!url.is_empty()) {
    DCHECK(url.is_valid());
    monster->GetAllCookiesForURLAsync(url, callback);
  } else {
    monster->GetAllCookiesAsync(callback);
  }
}

GURL GetURLFromCanonicalCookie(
    const net::CookieMonster::CanonicalCookie& cookie) {
  const std::string& domain_key = cookie.Domain();
  const std::string scheme =
      cookie.IsSecure() ? chrome::kHttpsScheme : chrome::kHttpScheme;
  const std::string host =
      domain_key.find('.') != 0 ? domain_key : domain_key.substr(1);
  return GURL(scheme + content::kStandardSchemeSeparator + host + "/");
}

void AppendMatchingCookiesToList(
    const net::CookieList& all_cookies,
    const std::string& store_id,
    const GURL& url, const DictionaryValue* details,
    const Extension* extension,
    ListValue* match_list) {
  net::CookieList::const_iterator it;
  for (it = all_cookies.begin(); it != all_cookies.end(); ++it) {
    // Ignore any cookie whose domain doesn't match the extension's
    // host permissions.
    GURL cookie_domain_url = GetURLFromCanonicalCookie(*it);
    if (!extension->HasHostPermission(cookie_domain_url))
      continue;
    // Filter the cookie using the match filter.
    extension_cookies_helpers::MatchFilter filter(details);
    if (filter.MatchesCookie(*it))
      match_list->Append(CreateCookieValue(*it, store_id));
  }
}

void AppendToTabIdList(Browser* browser, ListValue* tab_ids) {
  DCHECK(browser);
  DCHECK(tab_ids);
  TabStripModel* tab_strip = browser->tabstrip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_ids->Append(Value::CreateIntegerValue(
        ExtensionTabUtil::GetTabId(
            tab_strip->GetTabContentsAt(i)->web_contents())));
  }
}

MatchFilter::MatchFilter(const DictionaryValue* details)
    : details_(details) {
  DCHECK(details_);
}

bool MatchFilter::MatchesCookie(
    const net::CookieMonster::CanonicalCookie& cookie) {
  return MatchesString(keys::kNameKey, cookie.Name()) &&
         MatchesDomain(cookie.Domain()) &&
         MatchesString(keys::kPathKey, cookie.Path()) &&
         MatchesBoolean(keys::kSecureKey, cookie.IsSecure()) &&
         MatchesBoolean(keys::kSessionKey, !cookie.DoesExpire());
}

bool MatchFilter::MatchesString(const char* key, const std::string& value) {
  if (!details_->HasKey(key))
    return true;
  std::string filter_value;
  return (details_->GetString(key, &filter_value) &&
          value == filter_value);
}

bool MatchFilter::MatchesBoolean(const char* key, bool value) {
  if (!details_->HasKey(key))
    return true;
  bool filter_value = false;
  return (details_->GetBoolean(key, &filter_value) &&
          value == filter_value);
}

bool MatchFilter::MatchesDomain(const std::string& domain) {
  if (!details_->HasKey(keys::kDomainKey))
    return true;

  std::string filter_value;
  if (!details_->GetString(keys::kDomainKey, &filter_value))
    return false;
  // Add a leading '.' character to the filter domain if it doesn't exist.
  if (net::cookie_util::DomainIsHostOnly(filter_value))
    filter_value.insert(0, ".");

  std::string sub_domain(domain);
  // Strip any leading '.' character from the input cookie domain.
  if (!net::cookie_util::DomainIsHostOnly(sub_domain))
    sub_domain = sub_domain.substr(1);

  // Now check whether the domain argument is a subdomain of the filter domain.
  for (sub_domain.insert(0, ".");
       sub_domain.length() >= filter_value.length();) {
    if (sub_domain == filter_value)
      return true;
    const size_t next_dot = sub_domain.find('.', 1);  // Skip over leading dot.
    sub_domain.erase(0, next_dot);
  }
  return false;
}

}  // namespace extension_cookies_helpers
