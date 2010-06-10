// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Cookies API.

#include "chrome/browser/extensions/extension_cookies_api.h"

#include "base/json/json_writer.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_cookies_api_constants.h"
#include "chrome/browser/extensions/extension_cookies_helpers.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"
#include "net/base/cookie_monster.h"

namespace keys = extension_cookies_api_constants;

// static
ExtensionCookiesEventRouter* ExtensionCookiesEventRouter::GetInstance() {
  return Singleton<ExtensionCookiesEventRouter>::get();
}

void ExtensionCookiesEventRouter::Init() {
  if (registrar_.IsEmpty()) {
    registrar_.Add(this,
                   NotificationType::COOKIE_CHANGED,
                   NotificationService::AllSources());
  }
}

void ExtensionCookiesEventRouter::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::COOKIE_CHANGED:
      CookieChanged(
          Source<Profile>(source).ptr(),
          Details<ChromeCookieDetails>(details).ptr());
      break;

    default:
      NOTREACHED();
  }
}

void ExtensionCookiesEventRouter::CookieChanged(
    Profile* profile,
    ChromeCookieDetails* details) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetBoolean(keys::kRemovedKey, details->removed);
  dict->Set(
      keys::kCookieKey,
      extension_cookies_helpers::CreateCookieValue(*details->cookie_pair,
          extension_cookies_helpers::GetStoreIdFromProfile(profile)));
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  GURL cookie_domain =
      extension_cookies_helpers::GetURLFromCookiePair(*details->cookie_pair);
  DispatchEvent(profile, keys::kOnChanged, json_args, cookie_domain);
}

void ExtensionCookiesEventRouter::DispatchEvent(Profile* profile,
                                                const char* event_name,
                                                const std::string& json_args,
                                                GURL& cookie_domain) {
  if (profile && profile->GetExtensionMessageService()) {
    profile->GetExtensionMessageService()->DispatchEventToRenderers(
        event_name, json_args, profile->IsOffTheRecord(), cookie_domain);
  }
}

bool CookiesFunction::ParseUrl(const DictionaryValue* details, GURL* url,
                               bool check_host_permissions) {
  DCHECK(details && url);
  std::string url_string;
  // Get the URL string or return false.
  EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kUrlKey, &url_string));
  *url = GURL(url_string);
  if (!url->is_valid()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kInvalidUrlError, url_string);
    return false;
  }
  // Check against host permissions if needed.
  if (check_host_permissions &&
      !GetExtension()->HasHostPermission(*url)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNoHostPermissionsError, url->spec());
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
    // The store ID was explicitly specified in the details dictionary.
    // Retrieve its corresponding cookie store.
    std::string store_id_value;
    // Get the store ID string or return false.
    EXTENSION_FUNCTION_VALIDATE(
        details->GetString(keys::kStoreIdKey, &store_id_value));
    store_profile = extension_cookies_helpers::ChooseProfileFromStoreId(
        store_id_value, profile(), include_incognito());
    if (!store_profile) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          keys::kInvalidStoreIdError, store_id_value);
      return false;
    }
  } else {
    // The store ID was not specified; use the current execution context's
    // cookie store by default.
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
    *store_id =
        extension_cookies_helpers::GetStoreIdFromProfile(store_profile);
  return true;
}

bool GetCookieFunction::RunImpl() {
  // Return false if the arguments are malformed.
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  DCHECK(details);

  // Read/validate input parameters.
  GURL url;
  if (!ParseUrl(details, &url, true))
    return false;

  std::string name;
  // Get the cookie name string or return false.
  EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kNameKey, &name));

  net::CookieStore* cookie_store;
  std::string store_id;
  if (!ParseCookieStore(details, &cookie_store, &store_id))
    return false;
  DCHECK(cookie_store && !store_id.empty());

  net::CookieMonster::CookieList cookie_list =
      extension_cookies_helpers::GetCookieListFromStore(cookie_store, url);
  net::CookieMonster::CookieList::iterator it;
  for (it = cookie_list.begin(); it != cookie_list.end(); ++it) {
    // Return the first matching cookie. Relies on the fact that the
    // CookieMonster retrieves them in reverse domain-length order.
    const net::CookieMonster::CanonicalCookie& cookie = it->second;
    if (cookie.Name() == name) {
      result_.reset(
          extension_cookies_helpers::CreateCookieValue(*it, store_id));
      return true;
    }
  }
  // The cookie doesn't exist; return null.
  result_.reset(Value::CreateNullValue());
  return true;
}

bool GetAllCookiesFunction::RunImpl() {
  // Return false if the arguments are malformed.
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  DCHECK(details);

  // Read/validate input parameters.
  GURL url;
  if (details->HasKey(keys::kUrlKey) && !ParseUrl(details, &url, false))
    return false;

  net::CookieStore* cookie_store;
  std::string store_id;
  if (!ParseCookieStore(details, &cookie_store, &store_id))
    return false;
  DCHECK(cookie_store);

  ListValue* matching_list = new ListValue();
  extension_cookies_helpers::AppendMatchingCookiesToList(
      cookie_store, store_id, url, details, GetExtension(), matching_list);
  result_.reset(matching_list);
  return true;
}

bool SetCookieFunction::RunImpl() {
  // Return false if the arguments are malformed.
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  DCHECK(details);

  // Read/validate input parameters.
  GURL url;
  if (!ParseUrl(details, &url, true))
      return false;
  // The macros below return false if argument types are not as expected.
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
          url, name, value, domain, path, expiration_time, secure,
          http_only)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kCookieSetFailedError, name);
    return false;
  }
  return true;
}

bool RemoveCookieFunction::RunImpl() {
  // Return false if the arguments are malformed.
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  DCHECK(details);

  // Read/validate input parameters.
  GURL url;
  if (!ParseUrl(details, &url, true))
    return false;

  std::string name;
  // Get the cookie name string or return false.
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
  // Iterate through all browser instances, and for each browser,
  // add its tab IDs to either the regular or incognito tab ID list depending
  // whether the browser is regular or incognito.
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    Browser* browser = *iter;
    if (browser->profile() == original_profile) {
      extension_cookies_helpers::AppendToTabIdList(browser,
                                                   original_tab_ids.get());
    } else if (incognito_tab_ids.get() &&
               browser->profile() == incognito_profile) {
      extension_cookies_helpers::AppendToTabIdList(browser,
                                                   incognito_tab_ids.get());
    }
  }
  // Return a list of all cookie stores with at least one open tab.
  ListValue* cookie_store_list = new ListValue();
  if (original_tab_ids->GetSize() > 0) {
    cookie_store_list->Append(
        extension_cookies_helpers::CreateCookieStoreValue(
            original_profile, original_tab_ids.release()));
  }
  if (incognito_tab_ids.get() && incognito_tab_ids->GetSize() > 0) {
    cookie_store_list->Append(
        extension_cookies_helpers::CreateCookieStoreValue(
            incognito_profile, incognito_tab_ids.release()));
  }
  result_.reset(cookie_store_list);
  return true;
}
