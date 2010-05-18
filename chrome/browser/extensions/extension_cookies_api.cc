// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_cookies_api.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_cookies_api_constants.h"
#include "chrome/browser/extensions/extension_cookies_helpers.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/cookie_monster.h"

namespace helpers = extension_cookies_helpers;
namespace keys = extension_cookies_api_constants;

bool CookiesFunction::ParseUrl(const DictionaryValue* details, GURL* url) {
  DCHECK(details && url);
  std::string url_string;
  EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kUrlKey, &url_string));
  *url = GURL(url_string);
  if (!url->is_valid()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kInvalidUrlError, url_string);
    return false;
  }
  return true;
}

bool CookiesFunction::ParseCookieStore(const DictionaryValue* details,
                                       net::CookieStore** store,
                                       std::string* store_id) {
  DCHECK(details && (store || store_id));
  Profile* store_profile = NULL;
  if (details->HasKey(keys::kStoreIdKey)) {
    std::string store_id_value;
    EXTENSION_FUNCTION_VALIDATE(
        details->GetString(keys::kStoreIdKey, &store_id_value));
    store_profile = helpers::ChooseProfileFromStoreId(
        store_id_value, profile(), include_incognito());
    if (!store_profile) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          keys::kInvalidStoreIdError, store_id_value);
      return false;
    }
  } else {
    // GetCurrentBrowser() already takes into account incognito settings.
    Browser* current_browser = GetCurrentBrowser();
    if (!current_browser) {
      error_ = keys::kNoCookieStoreFoundError;
      return false;
    }
    store_profile = current_browser->profile();
  }
  DCHECK(store_profile);
  if (store)
    *store = store_profile->GetRequestContext()->GetCookieStore();
  if (store_id)
    *store_id = helpers::GetStoreIdFromProfile(store_profile);
  return true;
}

bool GetCookieFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* details = args_as_dictionary();
  DCHECK(details);

  // Read/validate input parameters.
  GURL url;
  if (!ParseUrl(details, &url))
    return false;
  if (!GetExtension()->HasHostPermission(url)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNoHostPermissionsError, url.spec());
    return false;
  }

  std::string name;
  EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kNameKey, &name));

  net::CookieStore* cookie_store;
  std::string store_id;
  if (!ParseCookieStore(details, &cookie_store, &store_id))
    return false;
  DCHECK(cookie_store && !store_id.empty());

  net::CookieMonster::CookieList cookie_list = helpers::GetCookieListFromStore(
      cookie_store, url);
  net::CookieMonster::CookieList::iterator it;
  for (it = cookie_list.begin(); it != cookie_list.end(); ++it) {
    // Return the first matching cookie; relies on the fact that the
    // CookieMonster retrieves them in reverse domain-length order.
    const net::CookieMonster::CanonicalCookie& cookie = it->second;
    if (cookie.Name() == name) {
      result_.reset(helpers::CreateCookieValue(*it, store_id));
      return true;
    }
  }
  // The cookie doesn't exist; return null.
  result_.reset(Value::CreateNullValue());
  return true;
}

bool GetAllCookiesFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* details = args_as_dictionary();

  // Read/validate input parameters.
  GURL url;
  if (details->HasKey(keys::kUrlKey) && !ParseUrl(details, &url))
    return false;

  net::CookieStore* cookie_store;
  std::string store_id;
  if (!ParseCookieStore(details, &cookie_store, &store_id))
    return false;
  DCHECK(cookie_store);

  ListValue* matching_list = new ListValue();
  helpers::AppendMatchingCookiesToList(cookie_store, store_id, url, details,
                                       GetExtension(), matching_list);
  result_.reset(matching_list);
  return true;
}

bool SetCookieFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* details = args_as_dictionary();

  // Read/validate input parameters.
  GURL url;
  if (!ParseUrl(details, &url))
      return false;
  if (!GetExtension()->HasHostPermission(url)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNoHostPermissionsError, url.spec());
    return false;
  }
  std::string name;
  if (details->HasKey(keys::kNameKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kNameKey, &name));
  }
  std::string value;
  if (details->HasKey(keys::kValueKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kValueKey, &value));
  }
  std::string domain;
  if (details->HasKey(keys::kDomainKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kDomainKey, &domain));
  }
  std::string path;
  if (details->HasKey(keys::kPathKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kPathKey, &path));
  }
  bool secure = false;
  if (details->HasKey(keys::kSecureKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetBoolean(keys::kSecureKey, &secure));
  }
  bool http_only = false;
  if (details->HasKey(keys::kHttpOnlyKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        details->GetBoolean(keys::kHttpOnlyKey, &http_only));
  }
  base::Time expiration_time;
  if (details->HasKey(keys::kExpirationDateKey)) {
    Value* expiration_date_value;
    EXTENSION_FUNCTION_VALIDATE(details->Get(keys::kExpirationDateKey,
                                             &expiration_date_value));
    double expiration_date;
    if (expiration_date_value->IsType(Value::TYPE_INTEGER)) {
      int expiration_date_int;
      EXTENSION_FUNCTION_VALIDATE(
          expiration_date_value->GetAsInteger(&expiration_date_int));
      expiration_date = static_cast<double>(expiration_date_int);
    } else {
      EXTENSION_FUNCTION_VALIDATE(
          expiration_date_value->GetAsReal(&expiration_date));
    }
    expiration_time = base::Time::FromDoubleT(expiration_date);
  }

  net::CookieStore* cookie_store;
  if (!ParseCookieStore(details, &cookie_store, NULL))
    return false;
  DCHECK(cookie_store);

  if (!cookie_store->GetCookieMonster()->SetCookieWithDetails(
      url, name, value, domain, path, expiration_time, secure, http_only)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kCookieSetFailedError, name);
    return false;
  }
  return true;
}

bool RemoveCookieFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* details = args_as_dictionary();

  // Read/validate input parameters.
  GURL url;
  if (!ParseUrl(details, &url))
    return false;
  if (!GetExtension()->HasHostPermission(url)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNoHostPermissionsError, url.spec());
    return false;
  }

  std::string name;
  EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kNameKey, &name));

  net::CookieStore* cookie_store;
  if (!ParseCookieStore(details, &cookie_store, NULL))
    return false;
  DCHECK(cookie_store);

  cookie_store->DeleteCookie(url, name);
  return true;
}

bool GetAllCookieStoresFunction::RunImpl() {
  Profile* original_profile = profile()->GetOriginalProfile();
  DCHECK(original_profile);
  scoped_ptr<ListValue> original_tab_ids(new ListValue());
  Profile* incognito_profile = NULL;
  scoped_ptr<ListValue> incognito_tab_ids;
  if (include_incognito()) {
    incognito_profile = profile()->GetOffTheRecordProfile();
    if (incognito_profile)
      incognito_tab_ids.reset(new ListValue());
  }
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    Browser* browser = *iter;
    if (browser->profile() == original_profile) {
      helpers::AppendToTabIdList(browser, original_tab_ids.get());
    } else if (incognito_tab_ids.get() &&
               browser->profile() == incognito_profile) {
      helpers::AppendToTabIdList(browser, incognito_tab_ids.get());
    }
  }
  ListValue* cookie_store_list = new ListValue();
  if (original_tab_ids->GetSize() > 0) {
    cookie_store_list->Append(helpers::CreateCookieStoreValue(
        original_profile, original_tab_ids.release()));
  }
  if (incognito_tab_ids.get() && incognito_tab_ids->GetSize() > 0) {
    cookie_store_list->Append(helpers::CreateCookieStoreValue(
        incognito_profile, incognito_tab_ids.release()));
  }
  result_.reset(cookie_store_list);
  return true;
}
