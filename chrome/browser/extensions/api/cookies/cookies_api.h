// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Cookies API functions for accessing internet
// cookies, as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/common/extensions/api/cookies.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/canonical_cookie.h"

namespace net {
class URLRequestContextGetter;
}

namespace extensions {

// Observes CookieMonster notifications and routes them as events to the
// extension system.
class CookiesEventRouter : public content::NotificationObserver {
 public:
  explicit CookiesEventRouter(Profile* profile);
  virtual ~CookiesEventRouter();

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Handler for the COOKIE_CHANGED event. The method takes the details of such
  // an event and constructs a suitable JSON formatted extension event from it.
  void CookieChanged(Profile* profile, ChromeCookieDetails* details);

  // This method dispatches events to the extension message service.
  void DispatchEvent(Profile* context,
                     const std::string& event_name,
                     scoped_ptr<base::ListValue> event_args,
                     GURL& cookie_domain);

  // Used for tracking registrations to CookieMonster notifications.
  content::NotificationRegistrar registrar_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CookiesEventRouter);
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

  // Constructs a GURL from the given url string. Returns false and assigns the
  // internal error_ value if the URL is invalid. If |check_host_permissions| is
  // true, the URL is also checked against the extension's host permissions, and
  // if there is no permission for the URL, this function returns false.
  bool ParseUrl(const std::string& url_string, GURL* url,
                bool check_host_permissions);

  // Gets the store identified by |store_id| and returns it in |context|.
  // If |store_id| contains an empty string, retrieves the current execution
  // context's store. In this case, |store_id| is populated with the found
  // store, and |context| can be NULL if the caller only wants |store_id|.
  bool ParseStoreContext(std::string* store_id,
                         net::URLRequestContextGetter** context);
};

// Implements the cookies.get() extension function.
class CookiesGetFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.get", COOKIES_GET)

  CookiesGetFunction();

 protected:
  virtual ~CookiesGetFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetCookieOnIOThread();
  void RespondOnUIThread();
  void GetCookieCallback(const net::CookieList& cookie_list);

  GURL url_;
  scoped_refptr<net::URLRequestContextGetter> store_context_;
  scoped_ptr<extensions::api::cookies::Get::Params> parsed_args_;
};

// Implements the cookies.getAll() extension function.
class CookiesGetAllFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.getAll", COOKIES_GETALL)

  CookiesGetAllFunction();

 protected:
  virtual ~CookiesGetAllFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetAllCookiesOnIOThread();
  void RespondOnUIThread();
  void GetAllCookiesCallback(const net::CookieList& cookie_list);

  GURL url_;
  scoped_refptr<net::URLRequestContextGetter> store_context_;
  scoped_ptr<extensions::api::cookies::GetAll::Params> parsed_args_;
};

// Implements the cookies.set() extension function.
class CookiesSetFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.set", COOKIES_SET)

  CookiesSetFunction();

 protected:
  virtual ~CookiesSetFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  void SetCookieOnIOThread();
  void RespondOnUIThread();
  void PullCookie(bool set_cookie_);
  void PullCookieCallback(const net::CookieList& cookie_list);

  GURL url_;
  bool success_;
  scoped_refptr<net::URLRequestContextGetter> store_context_;
  scoped_ptr<extensions::api::cookies::Set::Params> parsed_args_;
};

// Implements the cookies.remove() extension function.
class CookiesRemoveFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.remove", COOKIES_REMOVE)

  CookiesRemoveFunction();

 protected:
  virtual ~CookiesRemoveFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void RemoveCookieOnIOThread();
  void RespondOnUIThread();
  void RemoveCookieCallback();

  GURL url_;
  scoped_refptr<net::URLRequestContextGetter> store_context_;
  scoped_ptr<extensions::api::cookies::Remove::Params> parsed_args_;
};

// Implements the cookies.getAllCookieStores() extension function.
class CookiesGetAllCookieStoresFunction : public CookiesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.getAllCookieStores",
                             COOKIES_GETALLCOOKIESTORES)

 protected:
  virtual ~CookiesGetAllCookieStoresFunction() {}

  // ExtensionFunction:
  // CookiesGetAllCookieStoresFunction is sync.
  virtual void Run() OVERRIDE;
  virtual bool RunImpl() OVERRIDE;
};

class CookiesAPI : public ProfileKeyedAPI,
                   public extensions::EventRouter::Observer {
 public:
  explicit CookiesAPI(Profile* profile);
  virtual ~CookiesAPI();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<CookiesAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const extensions::EventListenerInfo& details)
      OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<CookiesAPI>;

  Profile* profile_;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "CookiesAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<CookiesEventRouter> cookies_event_router_;

  DISALLOW_COPY_AND_ASSIGN(CookiesAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_H_
