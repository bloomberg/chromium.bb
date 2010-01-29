// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_cookie_policy.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

ChromeCookiePolicy::ChromeCookiePolicy(Profile* profile) : profile_(profile) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  const DictionaryValue* cookie_exceptions_dict =
      profile_->GetPrefs()->GetDictionary(prefs::kCookieExceptions);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (cookie_exceptions_dict != NULL) {
    for (DictionaryValue::key_iterator i(cookie_exceptions_dict->begin_keys());
         i != cookie_exceptions_dict->end_keys(); ++i) {
      std::wstring wide_domain(*i);
      int content_settings = 0;
      bool success = cookie_exceptions_dict->GetIntegerWithoutPathExpansion(
          wide_domain, &content_settings);
      DCHECK(success);
      per_domain_policy_[WideToUTF8(wide_domain)] =
        static_cast<ContentPermissionType>(content_settings);
    }
  }
  net::CookiePolicy::set_type(net::CookiePolicy::FromInt(
      profile_->GetPrefs()->GetInteger(prefs::kCookieBehavior)));
}

// static
void ChromeCookiePolicy::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kCookieExceptions);
  prefs->RegisterIntegerPref(prefs::kCookieBehavior,
                             net::CookiePolicy::ALLOW_ALL_COOKIES);
}

void ChromeCookiePolicy::ResetToDefaults() {
  net::CookiePolicy::set_type(net::CookiePolicy::ALLOW_ALL_COOKIES);
  per_domain_policy_.clear();
  profile_->GetPrefs()->ClearPref(prefs::kCookieBehavior);
  profile_->GetPrefs()->ClearPref(prefs::kCookieExceptions);
}

bool ChromeCookiePolicy::CanGetCookies(const GURL& url,
    const GURL& first_party_for_cookies) {
  AutoLock auto_lock(lock_);
  ContentPermissionType permission = CheckPermissionForHost(url.host());
  switch (permission) {
    case CONTENT_PERMISSION_TYPE_DEFAULT:
      return net::CookiePolicy::CanGetCookies(url, first_party_for_cookies);

    case CONTENT_PERMISSION_TYPE_BLOCK:
      return false;

    case CONTENT_PERMISSION_TYPE_ALLOW:
      return true;

    case CONTENT_PERMISSION_TYPE_ASK:
      // TODO(darin): ask the user.

    default:
      NOTREACHED();
  }
  return false;  // To avoid compiler warnings.
}

bool ChromeCookiePolicy::CanSetCookie(const GURL& url,
    const GURL& first_party_for_cookies) {
  AutoLock auto_lock(lock_);
  ContentPermissionType permission = CheckPermissionForHost(url.host());
  switch (permission) {
    case  CONTENT_PERMISSION_TYPE_DEFAULT:
      return net::CookiePolicy::CanSetCookie(url, first_party_for_cookies);

    case CONTENT_PERMISSION_TYPE_BLOCK:
      return false;

    case CONTENT_PERMISSION_TYPE_ALLOW:
      return true;

    case CONTENT_PERMISSION_TYPE_ASK:
      // TODO(darin): ask the user.

    default:
      NOTREACHED();
  }
  return false;  // To avoid compiler warnings.
}

void ChromeCookiePolicy::SetPerDomainPermission(const std::string& domain,
                                                ContentPermissionType type) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (domain.empty())
    return;

  {
    AutoLock auto_lock(lock_);
    if (type == CONTENT_PERMISSION_TYPE_DEFAULT)
      per_domain_policy_.erase(domain);
    else
      per_domain_policy_[domain] = type;
  }

  DictionaryValue* cookie_exceptions_dict =
      profile_->GetPrefs()->GetMutableDictionary(prefs::kCookieExceptions);
  std::wstring wide_domain(UTF8ToWide(domain));
  if (type == CONTENT_PERMISSION_TYPE_DEFAULT) {
    cookie_exceptions_dict->RemoveWithoutPathExpansion(wide_domain, NULL);
  } else {
    cookie_exceptions_dict->SetWithoutPathExpansion(wide_domain,
        Value::CreateIntegerValue(type));
  }
}

void ChromeCookiePolicy::set_type(Type type) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  AutoLock auto_lock(lock_);
  net::CookiePolicy::set_type(type);
  profile_->GetPrefs()->SetInteger(prefs::kCookieBehavior, type);
}

ContentPermissionType ChromeCookiePolicy::CheckPermissionForHost(
    const std::string& host) const {
  std::string search_key(host);
  while (!search_key.empty()) {
    CookiePolicies::const_iterator i(per_domain_policy_.find(search_key));
    if (i != per_domain_policy_.end())
      return i->second;
    size_t dot_pos = search_key.find_first_of('.');
    if (dot_pos == std::string::npos)
      break;
    search_key = search_key.substr(dot_pos+1);
  }
  return CONTENT_PERMISSION_TYPE_DEFAULT;
}
