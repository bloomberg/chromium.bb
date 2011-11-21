// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_
#define CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

class ChromeURLDataManagerBackend;
class ChromeURLRequestContextFactory;
class IOThread;
class Profile;
class ProfileIOData;
namespace base {
class WaitableEvent;
}

// Subclass of net::URLRequestContext which can be used to store extra
// information for requests.
//
// All methods of this class must be called from the IO thread,
// including the constructor and destructor.
class ChromeURLRequestContext : public net::URLRequestContext {
 public:
  ChromeURLRequestContext();

  // Copies the state from |other| into this context.
  void CopyFrom(ChromeURLRequestContext* other);

  bool is_incognito() const {
    return is_incognito_;
  }

  virtual const std::string& GetUserAgent(const GURL& url) const OVERRIDE;

  // TODO(willchan): Get rid of the need for this accessor. Really, this should
  // move completely to ProfileIOData.
  ChromeURLDataManagerBackend* chrome_url_data_manager_backend() const;

  void set_is_incognito(bool is_incognito) {
    is_incognito_ = is_incognito;
  }

  void set_chrome_url_data_manager_backend(
      ChromeURLDataManagerBackend* backend);

  // Callback for when the accept language changes.
  void OnAcceptLanguageChange(const std::string& accept_language);

  // Callback for when the default charset changes.
  void OnDefaultCharsetChange(const std::string& default_charset);

 protected:
  virtual ~ChromeURLRequestContext();

 private:
  // ---------------------------------------------------------------------------
  // Important: When adding any new members below, consider whether they need to
  // be added to CopyFrom.
  // ---------------------------------------------------------------------------

  ChromeURLDataManagerBackend* chrome_url_data_manager_backend_;
  bool is_incognito_;

  // ---------------------------------------------------------------------------
  // Important: When adding any new members above, consider whether they need to
  // be added to CopyFrom.
  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContext);
};

// A net::URLRequestContextGetter subclass used by the browser. This returns a
// subclass of net::URLRequestContext which can be used to store extra
// information about requests.
//
// Most methods are expected to be called on the UI thread, except for
// the destructor and GetURLRequestContext().
class ChromeURLRequestContextGetter : public net::URLRequestContextGetter,
                                      public content::NotificationObserver {
 public:
  // Constructs a ChromeURLRequestContextGetter that will use |factory| to
  // create the ChromeURLRequestContext. If |profile| is non-NULL, then the
  // ChromeURLRequestContextGetter will additionally watch the preferences for
  // changes to charset/language and CleanupOnUIThread() will need to be
  // called to unregister.
  ChromeURLRequestContextGetter(Profile* profile,
                                ChromeURLRequestContextFactory* factory);

  // Note that GetURLRequestContext() can only be called from the IO
  // thread (it will assert otherwise). DONTUSEME_GetCookieStore() and
  // GetIOMessageLoopProxy however can be called from any thread.
  //
  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual net::CookieStore* DONTUSEME_GetCookieStore() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const OVERRIDE;

  // Convenience overload of GetURLRequestContext() that returns a
  // ChromeURLRequestContext* rather than a net::URLRequestContext*.
  ChromeURLRequestContext* GetIOContext() {
    return reinterpret_cast<ChromeURLRequestContext*>(GetURLRequestContext());
  }

  // Create an instance for use with an 'original' (non-OTR) profile. This is
  // expected to get called on the UI thread.
  static ChromeURLRequestContextGetter* CreateOriginal(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an original profile for media. This is expected to
  // get called on UI thread. This method takes a profile and reuses the
  // 'original' net::URLRequestContext for common files.
  static ChromeURLRequestContextGetter* CreateOriginalForMedia(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an original profile for extensions. This is expected
  // to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOriginalForExtensions(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an original profile for an app with isolated
  // storage. This is expected to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOriginalForIsolatedApp(
      Profile* profile,
      const ProfileIOData* profile_io_data,
      const std::string& app_id);

  // Create an instance for use with an OTR profile. This is expected to get
  // called on the UI thread.
  static ChromeURLRequestContextGetter* CreateOffTheRecord(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an OTR profile for extensions. This is expected
  // to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOffTheRecordForExtensions(
      Profile* profile, const ProfileIOData* profile_io_data);

  // Create an instance for an OTR profile for an app with isolated storage.
  // This is expected to get called on UI thread.
  static ChromeURLRequestContextGetter* CreateOffTheRecordForIsolatedApp(
      Profile* profile,
      const ProfileIOData* profile_io_data,
      const std::string& app_id);

  // Clean up UI thread resources. This is expected to get called on the UI
  // thread before the instance is deleted on the IO thread.
  void CleanupOnUIThread();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Must be called on the IO thread.
  virtual ~ChromeURLRequestContextGetter();

  // Registers an observer on |profile|'s preferences which will be used
  // to update the context when the default language and charset change.
  void RegisterPrefsObserver(Profile* profile);

  // These methods simply forward to the corresponding method on
  // ChromeURLRequestContext.
  void OnAcceptLanguageChange(const std::string& accept_language);
  void OnDefaultCharsetChange(const std::string& default_charset);
  void OnClearSiteDataOnExitChange(bool clear_site_data);

  // Saves the cookie store to |result| and signals |completion|.
  void GetCookieStoreAsyncHelper(base::WaitableEvent* completion,
                                 net::CookieStore** result);

  PrefChangeRegistrar registrar_;

  // |io_thread_| is always valid during the lifetime of |this| since |this| is
  // deleted on the IO thread.
  IOThread* const io_thread_;

  // Deferred logic for creating a ChromeURLRequestContext.
  // Access only from the IO thread.
  scoped_ptr<ChromeURLRequestContextFactory> factory_;

  // NULL if not yet initialized. Otherwise, it is the net::URLRequestContext
  // instance that was lazily created by GetURLRequestContext().
  // Access only from the IO thread.
  base::WeakPtr<net::URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestContextGetter);
};

#endif  // CHROME_BROWSER_NET_CHROME_URL_REQUEST_CONTEXT_H_
