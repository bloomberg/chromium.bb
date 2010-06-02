// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Cookies API functions for accessing internet
// cookies, as specified in chrome/common/extensions/api/extension_api.json.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_API_H_

#include <string>

#include "chrome/browser/extensions/extension_function.h"

namespace net {
class CookieStore;
}  // namespace net

// Serves as a base class for all cookies API functions, and defines some
// common functionality for parsing cookies API function arguments.
// Note that all of the functions in this file derive from ExtensionFunction,
// and are not threadsafe. They modify the result_ member variable directly;
// see chrome/browser/extensions/extension_function.h for more information.
class CookiesFunction : public SyncExtensionFunction {
 protected:
  // Looks for a 'url' value in the given details dictionary and constructs a
  // GURL from it. Returns false and assigns the internal error_ value if the
  // URL is invalid or isn't found in the dictionary. If check_host_permissions
  // is true, the URL is also checked against the extension's host permissions,
  // and if there is no permission for the URL, this function returns false.
  bool ParseUrl(const DictionaryValue* details, GURL* url,
                bool check_host_permissions);

  // Checks the given details dictionary for a 'storeId' value, and retrieves
  // the cookie store and the store ID associated with it.  If the 'storeId'
  // value isn't found in the dictionary, the current execution context's
  // cookie store is retrieved. Returns false on error and assigns the
  // internal error_ value if that occurs.
  // At least one of the output parameters store and store_id should be
  // non-null.
  bool ParseCookieStore(const DictionaryValue* details,
                        net::CookieStore** store, std::string* store_id);
};

// Implements the experimental.cookies.get() extension function.
class GetCookieFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.get")
};

// Implements the experimental.cookies.getAll() extension function.
class GetAllCookiesFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.getAll")
};

// Implements the experimental.cookies.set() extension function.
class SetCookieFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.set")
};

// Implements the experimental.cookies.remove() extension function.
class RemoveCookieFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.remove")
};

// Implements the experimental.cookies.getAllCookieStores() extension function.
class GetAllCookieStoresFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.getAllCookieStores")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_API_H_
