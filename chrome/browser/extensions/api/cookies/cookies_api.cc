// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Cookies API.

#include "chrome/browser/extensions/api/cookies/cookies_api.h"

#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/cookies/cookies_api_constants.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/cookies.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/error_utils.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using extensions::api::cookies::Cookie;
using extensions::api::cookies::CookieStore;

namespace Get = extensions::api::cookies::Get;
namespace GetAll = extensions::api::cookies::GetAll;
namespace GetAllCookieStores = extensions::api::cookies::GetAllCookieStores;
namespace Remove = extensions::api::cookies::Remove;
namespace Set = extensions::api::cookies::Set;

namespace extensions {
namespace keys = cookies_api_constants;

CookiesEventRouter::CookiesEventRouter(Profile* profile)
    : profile_(profile) {
  CHECK(registrar_.IsEmpty());
  registrar_.Add(this,
                 chrome::NOTIFICATION_COOKIE_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

CookiesEventRouter::~CookiesEventRouter() {
}

void CookiesEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  Profile* profile = content::Source<Profile>(source).ptr();
  if (!profile_->IsSameProfile(profile))
    return;

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

void CookiesEventRouter::CookieChanged(
    Profile* profile,
    ChromeCookieDetails* details) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* dict = new DictionaryValue();
  dict->SetBoolean(keys::kRemovedKey, details->removed);

  scoped_ptr<Cookie> cookie(
      cookies_helpers::CreateCookie(*details->cookie,
          cookies_helpers::GetStoreIdFromProfile(profile_)));
  dict->Set(keys::kCookieKey, cookie->ToValue().release());

  // Map the internal cause to an external string.
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

  args->Append(dict);

  GURL cookie_domain =
      cookies_helpers::GetURLFromCanonicalCookie(*details->cookie);
  DispatchEvent(profile, keys::kOnChanged, args.Pass(), cookie_domain);
}

void CookiesEventRouter::DispatchEvent(
    Profile* profile,
    const std::string& event_name,
    scoped_ptr<ListValue> event_args,
    GURL& cookie_domain) {
  EventRouter* router = profile ?
      extensions::ExtensionSystem::Get(profile)->event_router() : NULL;
  if (!router)
    return;
  scoped_ptr<Event> event(new Event(event_name, event_args.Pass()));
  event->restrict_to_profile = profile;
  event->event_url = cookie_domain;
  router->BroadcastEvent(event.Pass());
}

bool CookiesFunction::ParseUrl(const std::string& url_string, GURL* url,
                               bool check_host_permissions) {
  *url = GURL(url_string);
  if (!url->is_valid()) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kInvalidUrlError, url_string);
    return false;
  }
  // Check against host permissions if needed.
  if (check_host_permissions && !GetExtension()->HasHostPermission(*url)) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kNoHostPermissionsError, url->spec());
    return false;
  }
  return true;
}

bool CookiesFunction::ParseStoreContext(
    std::string* store_id,
    net::URLRequestContextGetter** context) {
  DCHECK((context || store_id->empty()));
  Profile* store_profile = NULL;
  if (!store_id->empty()) {
    store_profile = cookies_helpers::ChooseProfileFromStoreId(
        *store_id, profile(), include_incognito());
    if (!store_profile) {
      error_ = ErrorUtils::FormatErrorMessage(
          keys::kInvalidStoreIdError, *store_id);
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
    *store_id = cookies_helpers::GetStoreIdFromProfile(store_profile);
  }

  if (context)
    *context = store_profile->GetRequestContext();
  DCHECK(context);

  return true;
}

GetCookieFunction::GetCookieFunction() {
}

GetCookieFunction::~GetCookieFunction() {
}

bool GetCookieFunction::RunImpl() {
  parsed_args_ = Get::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parsed_args_.get());

  // Read/validate input parameters.
  if (!ParseUrl(parsed_args_->details.url, &url_, true))
    return false;

  std::string store_id = parsed_args_->details.store_id.get() ?
      *parsed_args_->details.store_id : "";
  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(&store_id, &store_context))
    return false;
  store_context_ = store_context;
  if (!parsed_args_->details.store_id.get())
    parsed_args_->details.store_id.reset(new std::string(store_id));

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
    if (it->Name() == parsed_args_->details.name) {
      scoped_ptr<Cookie> cookie(
          cookies_helpers::CreateCookie(*it, *parsed_args_->details.store_id));
      results_ = Get::Results::Create(*cookie);
      break;
    }
  }

  // The cookie doesn't exist; return null.
  if (it == cookie_list.end())
    SetResult(Value::CreateNullValue());

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetCookieFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void GetCookieFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

GetAllCookiesFunction::GetAllCookiesFunction() {
}

GetAllCookiesFunction::~GetAllCookiesFunction() {
}

bool GetAllCookiesFunction::RunImpl() {
  parsed_args_ = GetAll::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parsed_args_.get());

  if (parsed_args_->details.url.get() &&
      !ParseUrl(*parsed_args_->details.url, &url_, false)) {
    return false;
  }

  std::string store_id = parsed_args_->details.store_id.get() ?
      *parsed_args_->details.store_id : "";
  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(&store_id, &store_context))
    return false;
  store_context_ = store_context;
  if (!parsed_args_->details.store_id.get())
    parsed_args_->details.store_id.reset(new std::string(store_id));

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
  const extensions::Extension* extension = GetExtension();
  if (extension) {
    std::vector<linked_ptr<Cookie> > match_vector;
    cookies_helpers::AppendMatchingCookiesToVector(
        cookie_list, url_, &parsed_args_->details,
        GetExtension(), &match_vector);

    results_ = GetAll::Results::Create(match_vector);
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

SetCookieFunction::SetCookieFunction() : success_(false) {
}

SetCookieFunction::~SetCookieFunction() {
}

bool SetCookieFunction::RunImpl() {
  parsed_args_ = Set::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parsed_args_.get());

  // Read/validate input parameters.
  if (!ParseUrl(parsed_args_->details.url, &url_, true))
      return false;

  std::string store_id = parsed_args_->details.store_id.get() ?
      *parsed_args_->details.store_id : "";
  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(&store_id, &store_context))
    return false;
  store_context_ = store_context;
  if (!parsed_args_->details.store_id.get())
    parsed_args_->details.store_id.reset(new std::string(store_id));

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

  base::Time expiration_time;
  if (parsed_args_->details.expiration_date.get()) {
    // Time::FromDoubleT converts double time 0 to empty Time object. So we need
    // to do special handling here.
    expiration_time = (*parsed_args_->details.expiration_date == 0) ?
        base::Time::UnixEpoch() :
        base::Time::FromDoubleT(*parsed_args_->details.expiration_date);
  }

  cookie_monster->SetCookieWithDetailsAsync(
      url_,
      parsed_args_->details.name.get() ? *parsed_args_->details.name : "",
      parsed_args_->details.value.get() ? *parsed_args_->details.value : "",
      parsed_args_->details.domain.get() ? *parsed_args_->details.domain : "",
      parsed_args_->details.path.get() ? *parsed_args_->details.path : "",
      expiration_time,
      parsed_args_->details.secure.get() ?
          *parsed_args_->details.secure.get() :
          false,
      parsed_args_->details.http_only.get() ?
          *parsed_args_->details.http_only :
          false,
      base::Bind(&SetCookieFunction::PullCookie, this));
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
    std::string name = parsed_args_->details.name.get() ?
        *parsed_args_->details.name : "";
    if (it->Name() == name) {
      scoped_ptr<Cookie> cookie(
          cookies_helpers::CreateCookie(*it, *parsed_args_->details.store_id));
      results_ = Set::Results::Create(*cookie);
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
    std::string name = parsed_args_->details.name.get() ?
        *parsed_args_->details.name : "";
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kCookieSetFailedError, name);
  }
  SendResponse(success_);
}

RemoveCookieFunction::RemoveCookieFunction() {
}

RemoveCookieFunction::~RemoveCookieFunction() {
}

bool RemoveCookieFunction::RunImpl() {
  parsed_args_ = Remove::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parsed_args_.get());

  // Read/validate input parameters.
  if (!ParseUrl(parsed_args_->details.url, &url_, true))
    return false;

  std::string store_id = parsed_args_->details.store_id.get() ?
      *parsed_args_->details.store_id : "";
  net::URLRequestContextGetter* store_context = NULL;
  if (!ParseStoreContext(&store_id, &store_context))
    return false;
  store_context_ = store_context;
  if (!parsed_args_->details.store_id.get())
    parsed_args_->details.store_id.reset(new std::string(store_id));

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
      url_, parsed_args_->details.name,
      base::Bind(&RemoveCookieFunction::RemoveCookieCallback, this));
}

void RemoveCookieFunction::RemoveCookieCallback() {
  // Build the callback result
  Remove::Results::Details details;
  details.name = parsed_args_->details.name;
  details.url = url_.spec();
  details.store_id = *parsed_args_->details.store_id;
  results_ = Remove::Results::Create(details);

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
      cookies_helpers::AppendToTabIdList(browser, original_tab_ids.get());
    } else if (incognito_tab_ids.get() &&
               browser->profile() == incognito_profile) {
      cookies_helpers::AppendToTabIdList(browser, incognito_tab_ids.get());
    }
  }
  // Return a list of all cookie stores with at least one open tab.
  std::vector<linked_ptr<CookieStore> > cookie_stores;
  if (original_tab_ids->GetSize() > 0) {
    cookie_stores.push_back(make_linked_ptr(
        cookies_helpers::CreateCookieStore(
            original_profile, original_tab_ids.release()).release()));
  }
  if (incognito_tab_ids.get() && incognito_tab_ids->GetSize() > 0 &&
      incognito_profile) {
    cookie_stores.push_back(make_linked_ptr(
        cookies_helpers::CreateCookieStore(
            incognito_profile, incognito_tab_ids.release()).release()));
  }
  results_ = GetAllCookieStores::Results::Create(cookie_stores);
  return true;
}

void GetAllCookieStoresFunction::Run() {
  SendResponse(RunImpl());
}

CookiesAPI::CookiesAPI(Profile* profile)
    : profile_(profile) {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, keys::kOnChanged);
}

CookiesAPI::~CookiesAPI() {
}

void CookiesAPI::Shutdown() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

static base::LazyInstance<ProfileKeyedAPIFactory<CookiesAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<CookiesAPI>* CookiesAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void CookiesAPI::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  cookies_event_router_.reset(new CookiesEventRouter(profile_));
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

}  // namespace extensions
