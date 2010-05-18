// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_HELPERS_H_

#include <string>

#include "base/values.h"
#include "net/base/cookie_monster.h"

class Browser;
class Extension;
class Profile;

namespace extension_cookies_helpers {

// Returns either the original profile or the incognito profile, based on the
// given store ID.  Returns NULL if the profile doesn't exist or is not allowed
// (e.g. if incognito is not enabled for the extension).
Profile* ChooseProfileFromStoreId(const std::string& store_id,
                                  Profile* profile,
                                  bool include_incognito);

std::string GetStoreIdFromProfile(Profile* profile);

// Constructs a Cookie object as defined by the cookies API.
DictionaryValue* CreateCookieValue(
    const net::CookieMonster::CookieListPair& cookie_pair,
    const std::string& store_id);

// Constructs a CookieStore object as defined by the cookies API.
DictionaryValue* CreateCookieStoreValue(Profile* profile,
                                        ListValue* tab_ids);

// Retrieves all cookies from the given cookie store corresponding to the given
// URL. If the URL is empty, all cookies in the cookie store are retrieved.
net::CookieMonster::CookieList GetCookieListFromStore(
    net::CookieStore* cookie_store, const GURL& url);

// Constructs a URL from a cookie's information for use in checking
// a cookie against the extension's host permissions. The Secure
// property of the cookie defines the URL scheme, and the cookie's
// domain becomes the URL host.
GURL GetURLFromCookiePair(
    const net::CookieMonster::CookieListPair& cookie_pair);

// Looks through all cookies in the given cookie store, and appends to the
// match list all the cookies that both match the given URL and cookie details
// and are allowed by extension host permissions.
void AppendMatchingCookiesToList(
    net::CookieStore* cookie_store, const std::string& store_id,
    const GURL& url, const DictionaryValue* details,
    const Extension* extension,
    ListValue* match_list);

// Appends the IDs of all tabs belonging to the given browser to the
// given list.
void AppendToTabIdList(Browser* browser, ListValue* tab_ids);

// A class representing the cookie filter parameters passed into
// cookies.getAll().
// This class is essentially a convenience wrapper for the details dictionary
// passed into the cookies.getAll() API by the user. If the dictionary contains
// no filter parameters, the MatchFilter will always trivially
// match all cookies.
class MatchFilter {
 public:
  // Takes the details dictionary argument given by the user as input.
  // This class does not take ownership of the lifetime of the DictionaryValue
  // object.
  explicit MatchFilter(const DictionaryValue* details);

  // Returns true if the given cookie matches the properties in the match
  // filter.
  bool MatchesCookie(const net::CookieMonster::CookieListPair& cookie_pair);

 private:
  bool MatchesString(const wchar_t* key, const std::string& value);
  bool MatchesBoolean(const wchar_t* key, bool value);

  // Returns true if the given cookie domain string matches the filter's
  // domain. Any cookie domain which is equal to or is a subdomain of the
  // filter's domain will be matched; leading '.' characters indicating
  // host-only domains have no meaning in the match filter domain (for
  // instance, a match filter domain of 'foo.bar.com' will be treated the same
  // as '.foo.bar.com', and both will match cookies with domain values of
  // 'foo.bar.com', '.foo.bar.com', and 'baz.foo.bar.com'.
  bool MatchesDomain(const std::string& domain);

  const DictionaryValue* details_;
};

}  // namespace extension_cookies_helpers

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_HELPERS_H_
