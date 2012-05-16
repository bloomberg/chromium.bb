// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Cookies API functions for accessing internet
// cookies, as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

namespace base {
class DictionaryValue;
}

namespace net {
class CookieList;
class URLRequestContextGetter;
}

namespace extensions {

// Observes CookieMonster notifications and routes them as events to the
// extension system.
class ExtensionCookiesEventRouter : public content::NotificationObserver {
 public:
  explicit ExtensionCookiesEventRouter(Profile* profile);
  virtual ~ExtensionCookiesEventRouter();

  void Init();

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Handler for the COOKIE_CHANGED event. The method takes the details of such
  // an event and constructs a suitable JSON formatted extension event from it.
  void CookieChanged(Profile* profile,
                     ChromeCookieDetails* details);

  // This method dispatches events to the extension message service.
  void DispatchEvent(Profile* context,
                     const char* event_name,
                     const std::string& json_args,
                     GURL& cookie_domain);

  // Used for tracking registrations to CookieMonster notifications.
  content::NotificationRegistrar registrar_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCookiesEventRouter);
};

// Serves as a base class for all cookies API functions, and defines some
// common functionality for parsing cookies API function arguments.
// Note that all of the functions in this file derive from
// AsyncExtensionFunction, and are not threadsafe, so they should not be
// concurrently accessed from multiple threads. They modify |result_| and other
// member variables directly.
// See chrome/browser/extensions/extension_function.h for more information.
class CookiesFunction : public AsyncExtensionFunction {
 protected:
  virtual ~CookiesFunction() {}

  // Looks for a 'url' value in the given details dictionary and constructs a
  // GURL from it. Returns false and assigns the internal error_ value if the
  // URL is invalid or isn't found in the dictionary. If check_host_permissions
  // is true, the URL is also checked against the extension's host permissions,
  // and if there is no permission for the URL, this function returns false.
  bool ParseUrl(const base::DictionaryValue* details, GURL* url,
                bool check_host_permissions);

  // Checks the given details dictionary for a 'storeId' value, and retrieves
  // the cookie store context and the store ID associated with it.  If the
  // 'storeId' value isn't found in the dictionary, the current execution
  // context's cookie store context is retrieved. Returns false on error and
  // assigns the internal error_ value if that occurs.
  // At least one of the output parameters store and store_id should be
  // non-NULL.
  bool ParseStoreContext(const base::DictionaryValue* details,
                         net::URLRequestContextGetter** context,
                         std::string* store_id);
};

// Implements the cookies.get() extension function.
class GetCookieFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("cookies.get")

  GetCookieFunction();

 protected:
  virtual ~GetCookieFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetCookieOnIOThread();
  void RespondOnUIThread();
  void GetCookieCallback(const net::CookieList& cookie_list);

  std::string name_;
  GURL url_;
  std::string store_id_;
  scoped_refptr<net::URLRequestContextGetter> store_context_;
};

// Implements the cookies.getAll() extension function.
class GetAllCookiesFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("cookies.getAll")

  GetAllCookiesFunction();

 protected:
  virtual ~GetAllCookiesFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetAllCookiesOnIOThread();
  void RespondOnUIThread();
  void GetAllCookiesCallback(const net::CookieList& cookie_list);

  base::DictionaryValue* details_;
  GURL url_;
  std::string store_id_;
  scoped_refptr<net::URLRequestContextGetter> store_context_;
};

// Implements the cookies.set() extension function.
class SetCookieFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("cookies.set")

  SetCookieFunction();

 protected:
  virtual ~SetCookieFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  void SetCookieOnIOThread();
  void RespondOnUIThread();
  void PullCookie(bool set_cookie_);
  void PullCookieCallback(const net::CookieList& cookie_list);

  GURL url_;
  std::string name_;
  std::string value_;
  std::string domain_;
  std::string path_;
  bool secure_;
  bool http_only_;
  base::Time expiration_time_;
  bool success_;
  std::string store_id_;
  scoped_refptr<net::URLRequestContextGetter> store_context_;
};

// Implements the cookies.remove() extension function.
class RemoveCookieFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("cookies.remove")

  RemoveCookieFunction();

 protected:
  virtual ~RemoveCookieFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void RemoveCookieOnIOThread();
  void RespondOnUIThread();
  void RemoveCookieCallback();

  GURL url_;
  std::string name_;
  bool success_;
  std::string store_id_;
  scoped_refptr<net::URLRequestContextGetter> store_context_;
};

// Implements the cookies.getAllCookieStores() extension function.
class GetAllCookieStoresFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("cookies.getAllCookieStores")

 protected:
  virtual ~GetAllCookieStoresFunction() {}

  // ExtensionFunction:
  // GetAllCookieStoresFunction is sync.
  virtual void Run() OVERRIDE;
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_H_
