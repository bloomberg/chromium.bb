// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_API_H_

#include <string>

#include "chrome/browser/extensions/extension_function.h"

namespace net {
class CookieStore;
}  // namespace net

class CookiesFunction : public SyncExtensionFunction {
 protected:
  // Looks for a 'url' value in the given details dictionary and constructs a
  // GURL from it. Returns false and assigns the internal error_ value if the
  // URL is invalid or isn't found in the dictionary.
  bool ParseUrl(const DictionaryValue* details, GURL* url);

  // Checks the given details dictionary for a 'storeId' value, and retrieves
  // the cookie store and the store ID associated with it.  If the 'storeId'
  // value isn't found in the dictionary, the current execution context's
  // cookie store is retrieved. Returns false on error, and assigns the
  // internal error_ value if that occurs.
  // At least one of the output parameters store and store_id should be
  // non-null.
  bool ParseCookieStore(const DictionaryValue* details,
                        net::CookieStore** store, std::string* store_id);
};

class GetCookieFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.get")
};
class GetAllCookiesFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.getAll")
};
class SetCookieFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.set")
};
class RemoveCookieFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.remove")
};
class GetAllCookieStoresFunction : public CookiesFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.cookies.getAllCookieStores")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_API_H_
