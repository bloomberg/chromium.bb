// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Cookies API.

#include "chrome/browser/extensions/api/cookies/cookies_api.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/cookies/cookies_api_constants.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace extensions {
namespace keys = cookies_api_constants;

ExtensionCookiesEventRouter::ExtensionCookiesEventRouter(Profile* profile)
    : profile_(profile) {}

ExtensionCookiesEventRouter::~ExtensionCookiesEventRouter() {}

void ExtensionCookiesEventRouter::Init() {
  CHECK(registrar_.IsEmpty());
  registrar_.Add(this,
                 chrome::NOTIFICATION_COOKIE_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void ExtensionCookiesEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  Profile* profile = content::Source<Profile>(source).ptr();
  if (!profile_->IsSameProfile(profile)) {
    return;
  }
  switch (type) {
    case chrome::NOTIFICATION_COOKIE_CHANGED:
      CookieChanged(
          profile,
          content::Details<ChromeCookieDetails>(details).ptr());
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
      cookies_helpers::CreateCookieValue(*details->cookie,
          cookies_helpers::GetStoreIdFromProfile(profile)));

  // Map the interal cause to an external string.
  std::string cause;
  switch (details->cause) {
    case net::CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT:
      cause = keys::kExplicitChangeCause;
      break;

    case net::CookieMonster::Delegate::CHANGE_COOKIE_OVERWRITE:
      cause = keys::kOverwriteChangeCause;
      break;

    case net::CookieMonster::Delegate::CHANGE_COOKIE_EXPIRED:
      cause = keys::kExpiredChangeCause;
      break;

    case net::CookieMonster::Delegate::CHANGE_COOKIE_EVICTED:
      cause = keys::kEvictedChangeCause;
      break;

    case net::CookieMonster::Delegate::CHANGE_COOKIE_EXPIRED_OVERWRITE:
      cause = keys::kExpiredOverwriteChangeCause;
      break;

    default:
      NOTREACHED();
  }
  dict->SetString(keys::kCauseKey, cause);

  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);
  GURL cookie_domain =
      cookies_helpers::GetURLFromCanonicalCookie(*details->cookie);
  DispatchEvent(profile, keys::kOnChanged, json_args, cookie_domain);
}

void ExtensionCookiesEventRouter::DispatchEvent(Profile* profile,
                                                const char* event_name,
                                                const std::string& json_args,
                                                GURL& cookie_domain) {
  if (profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, json_args, profile, cookie_domain);
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

bool CookiesFunction::ParseStoreContext(const DictionaryValue* details,
                                        net::URLRequestContextGetter** context,
                                        std::string* store_id) {
  DCHECK(details && (context || store_id));
  Profile* store_profile = NULL;
  if (details->HasKey(keys::kStoreIdKey)) {
    // The store ID was explicitly specified in the details dictionary.
    // Retrieve its corresponding cookie store.
    std::string store_id_value;
    // Get the store ID string or return false.
    EXTENSION_FUNCTION_VALIDATE(
        details->GetString(keys::kStoreIdKey, &store_id_value));
    store_profile = cookies_helpers::ChooseProfileFromStoreId(
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

  if (context)
    *context = store_profile->GetRequestContext();
  if (store_id)
    *store_id = cookies_helpers::GetStoreIdFromProfile(store_profile);

  return true;
}

GetCookieFunction::GetCookieFunction() {}

GetCookieFunction::~GetCookieFunction() {}

bool GetCookieFunction::RunImpl() {
  // Return false if the arguments are malformed.
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  DCHECK(details);

  // Read/validate input parameters.
  if (!ParseUrl(details, &url_, true))
    return false;

  // Get the cookie name string or return false.
  EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kNameKey, &name_));

  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(details, &store_context, &store_id_))
    return false;

  DCHECK(store_context && !store_id_.empty());
  store_context_ = store_context;

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetCookieFunction::GetCookieOnIOThread, this));
  DCHECK(rv);

  // Will finish asynchronously.
  return true;
}

void GetCookieFunction::GetCookieOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* cookie_store =
      store_context_->GetURLRequestContext()->cookie_store();
  cookies_helpers::GetCookieListFromStore(
      cookie_store, url_,
      base::Bind(&GetCookieFunction::GetCookieCallback, this));
}

void GetCookieFunction::GetCookieCallback(const net::CookieList& cookie_list) {
  net::CookieList::const_iterator it;
  for (it = cookie_list.begin(); it != cookie_list.end(); ++it) {
    // Return the first matching cookie. Relies on the fact that the
    // CookieMonster returns them in canonical order (longest path, then
    // earliest creation time).
    if (it->Name() == name_) {
      result_.reset(
          cookies_helpers::CreateCookieValue(*it, store_id_));
      break;
    }
  }

  // The cookie doesn't exist; return null.
  if (it == cookie_list.end())
    result_.reset(Value::CreateNullValue());

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetCookieFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void GetCookieFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

GetAllCookiesFunction::GetAllCookiesFunction() : details_(NULL) {}

GetAllCookiesFunction::~GetAllCookiesFunction() {}

bool GetAllCookiesFunction::RunImpl() {
  // Return false if the arguments are malformed.
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details_));
  DCHECK(details_);

  // Read/validate input parameters.
  if (details_->HasKey(keys::kUrlKey) && !ParseUrl(details_, &url_, false))
    return false;

  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(details_, &store_context, &store_id_))
    return false;
  DCHECK(store_context);
  store_context_ = store_context;

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetAllCookiesFunction::GetAllCookiesOnIOThread, this));
  DCHECK(rv);

  // Will finish asynchronously.
  return true;
}

void GetAllCookiesFunction::GetAllCookiesOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* cookie_store =
      store_context_->GetURLRequestContext()->cookie_store();
  cookies_helpers::GetCookieListFromStore(
      cookie_store, url_,
      base::Bind(&GetAllCookiesFunction::GetAllCookiesCallback, this));
}

void GetAllCookiesFunction::GetAllCookiesCallback(
    const net::CookieList& cookie_list) {
  const Extension* extension = GetExtension();
  if (extension) {
    ListValue* matching_list = new ListValue();
    cookies_helpers::AppendMatchingCookiesToList(
        cookie_list, store_id_, url_, details_,
        GetExtension(), matching_list);
    result_.reset(matching_list);
  }
  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetAllCookiesFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void GetAllCookiesFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

SetCookieFunction::SetCookieFunction()
    : secure_(false),
      http_only_(false),
      success_(false) {
}

SetCookieFunction::~SetCookieFunction() {
}

bool SetCookieFunction::RunImpl() {
  // Return false if the arguments are malformed.
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  DCHECK(details);

  // Read/validate input parameters.
  if (!ParseUrl(details, &url_, true))
      return false;
  // The macros below return false if argument types are not as expected.
  if (details->HasKey(keys::kNameKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kNameKey, &name_));
  if (details->HasKey(keys::kValueKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kValueKey, &value_));
  if (details->HasKey(keys::kDomainKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kDomainKey, &domain_));
  if (details->HasKey(keys::kPathKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kPathKey, &path_));

  if (details->HasKey(keys::kSecureKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        details->GetBoolean(keys::kSecureKey, &secure_));
  }
  if (details->HasKey(keys::kHttpOnlyKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        details->GetBoolean(keys::kHttpOnlyKey, &http_only_));
  }
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
          expiration_date_value->GetAsDouble(&expiration_date));
    }
    // Time::FromDoubleT converts double time 0 to empty Time object. So we need
    // to do special handling here.
    expiration_time_ = (expiration_date == 0) ?
        base::Time::UnixEpoch() : base::Time::FromDoubleT(expiration_date);
  }

  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(details, &store_context, NULL))
    return false;
  DCHECK(store_context);
  store_context_ = store_context;

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SetCookieFunction::SetCookieOnIOThread, this));
  DCHECK(rv);

  // Will finish asynchronously.
  return true;
}

void SetCookieFunction::SetCookieOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieMonster* cookie_monster =
      store_context_->GetURLRequestContext()->cookie_store()->
      GetCookieMonster();
  cookie_monster->SetCookieWithDetailsAsync(
      url_, name_, value_, domain_, path_, expiration_time_,
      secure_, http_only_, base::Bind(&SetCookieFunction::PullCookie, this));
}

void SetCookieFunction::PullCookie(bool set_cookie_result) {
  // Pull the newly set cookie.
  net::CookieMonster* cookie_monster =
      store_context_->GetURLRequestContext()->cookie_store()->
      GetCookieMonster();
  success_ = set_cookie_result;
  cookies_helpers::GetCookieListFromStore(
      cookie_monster, url_,
      base::Bind(&SetCookieFunction::PullCookieCallback, this));
}

void SetCookieFunction::PullCookieCallback(const net::CookieList& cookie_list) {
  net::CookieList::const_iterator it;
  for (it = cookie_list.begin(); it != cookie_list.end(); ++it) {
    // Return the first matching cookie. Relies on the fact that the
    // CookieMonster returns them in canonical order (longest path, then
    // earliest creation time).
    if (it->Name() == name_) {
      result_.reset(
          cookies_helpers::CreateCookieValue(*it, store_id_));
      break;
    }
  }

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SetCookieFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SetCookieFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!success_) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kCookieSetFailedError, name_);
  }
  SendResponse(success_);
}

RemoveCookieFunction::RemoveCookieFunction() : success_(false) {
}

RemoveCookieFunction::~RemoveCookieFunction() {
}

bool RemoveCookieFunction::RunImpl() {
  // Return false if the arguments are malformed.
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  DCHECK(details);

  // Read/validate input parameters.
  if (!ParseUrl(details, &url_, true))
    return false;

  // Get the cookie name string or return false.
  EXTENSION_FUNCTION_VALIDATE(details->GetString(keys::kNameKey, &name_));

  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(details, &store_context, &store_id_))
    return false;
  DCHECK(store_context);
  store_context_ = store_context;

  // Pass the work off to the IO thread.
  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RemoveCookieFunction::RemoveCookieOnIOThread, this));
  DCHECK(rv);

  // Will return asynchronously.
  return true;
}

void RemoveCookieFunction::RemoveCookieOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Remove the cookie
  net::CookieStore* cookie_store =
      store_context_->GetURLRequestContext()->cookie_store();
  cookie_store->DeleteCookieAsync(
      url_, name_,
      base::Bind(&RemoveCookieFunction::RemoveCookieCallback, this));
}

void RemoveCookieFunction::RemoveCookieCallback() {
  // Build the callback result
  DictionaryValue* resultDictionary = new DictionaryValue();
  resultDictionary->SetString(keys::kNameKey, name_);
  resultDictionary->SetString(keys::kUrlKey, url_.spec());
  resultDictionary->SetString(keys::kStoreIdKey, store_id_);
  result_.reset(resultDictionary);

  // Return to UI thread
  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemoveCookieFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void RemoveCookieFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

bool GetAllCookieStoresFunction::RunImpl() {
  Profile* original_profile = profile();
  DCHECK(original_profile);
  scoped_ptr<ListValue> original_tab_ids(new ListValue());
  Profile* incognito_profile = NULL;
  scoped_ptr<ListValue> incognito_tab_ids;
  if (include_incognito() && profile()->HasOffTheRecordProfile()) {
    incognito_profile = profile()->GetOffTheRecordProfile();
    if (incognito_profile)
      incognito_tab_ids.reset(new ListValue());
  }
  DCHECK(original_profile != incognito_profile);

  // Iterate through all browser instances, and for each browser,
  // add its tab IDs to either the regular or incognito tab ID list depending
  // whether the browser is regular or incognito.
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    Browser* browser = *iter;
    if (browser->profile() == original_profile) {
      cookies_helpers::AppendToTabIdList(browser,
                                                   original_tab_ids.get());
    } else if (incognito_tab_ids.get() &&
               browser->profile() == incognito_profile) {
      cookies_helpers::AppendToTabIdList(browser,
                                                   incognito_tab_ids.get());
    }
  }
  // Return a list of all cookie stores with at least one open tab.
  ListValue* cookie_store_list = new ListValue();
  if (original_tab_ids->GetSize() > 0) {
    cookie_store_list->Append(
        cookies_helpers::CreateCookieStoreValue(
            original_profile, original_tab_ids.release()));
  }
  if (incognito_tab_ids.get() && incognito_tab_ids->GetSize() > 0 &&
      incognito_profile) {
    cookie_store_list->Append(
        cookies_helpers::CreateCookieStoreValue(
            incognito_profile, incognito_tab_ids.release()));
  }
  result_.reset(cookie_store_list);
  return true;
}

void GetAllCookieStoresFunction::Run() {
  SendResponse(RunImpl());
}

}  // namespace extensions
