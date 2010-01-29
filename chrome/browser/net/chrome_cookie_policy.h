// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_COOKIE_POLICY_H_
#define CHROME_BROWSER_NET_CHROME_COOKIE_POLICY_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/lock.h"
#include "chrome/common/content_permission_types.h"
#include "net/base/cookie_policy.h"

class GURL;
class PrefService;
class Profile;

// The ChromeCookiePolicy class implements per-domain cookie policies.
class ChromeCookiePolicy : public net::CookiePolicy {
 public:
  typedef std::map<std::string, ContentPermissionType> CookiePolicies;

  explicit ChromeCookiePolicy(Profile* profile);

  virtual ~ChromeCookiePolicy() {}

  static void RegisterUserPrefs(PrefService* prefs);

  void ResetToDefaults();

  // Consult the user's cookie blocking preferences to determine whether the
  // URL's cookies can be read.
  virtual bool CanGetCookies(const GURL& url,
                             const GURL& first_party_for_cookies);

  // Consult the user's cookie blocking preferences to determine whether the
  // URL's cookies can be set.
  virtual bool CanSetCookie(const GURL& url,
                            const GURL& first_party_for_cookies);

  // Sets per domain policies. A policy of type DEFAULT will erase the entry.
  //
  // This should be called only on the UI thread.
  void SetPerDomainPermission(const std::string& domain,
                              ContentPermissionType type);

  // Returns all per domain policies.
  //
  // This can be called on any thread.
  CookiePolicies GetAllPerDomainPermissions() const {
    return per_domain_policy_;
  }

  // Sets the current policy to enforce.
  //
  // This should be called only on the UI thread.
  void set_type(Type type);

  // This can be called on any thread.
  Type type() const {
    AutoLock auto_lock(lock_);
    return net::CookiePolicy::type();
  }

 protected:
  ContentPermissionType CheckPermissionForHost(const std::string& host) const;

  Profile* profile_;

  mutable Lock lock_;

  // Local copy of prefs.
  CookiePolicies per_domain_policy_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeCookiePolicy);
};

#endif  // CHROME_BROWSER_NET_CHROME_COOKIE_POLICY_H_
